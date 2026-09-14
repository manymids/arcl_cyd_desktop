// Host tests for bridge/deploy.mjs. No device: a fake client records what would
// be sent and answers the few queries the deploy flow makes.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdtemp, mkdir, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { ManifestError } from './manifest.mjs';
import {
  CHUNK_BYTES, DeployError, collectApp, deployApp, pinApp, resolveInside, uploadFile,
} from './deploy.mjs';

class FakeClient {
  constructor({ attachedPolls = 0, shortcuts = [] } = {}) {
    this.requests = [];
    this.attachedPolls = attachedPolls;
    this.shortcuts = shortcuts;
  }

  async request(command, parameters = {}) {
    this.requests.push({ command, ...parameters });
    if (command === 'desktop_diagnostics') {
      const attached = this.attachedPolls > 0;
      this.attachedPolls -= 1;
      return { runtime_watchdog_attached: attached };
    }
    if (command === 'desktop_shortcuts_list') {
      const offset = parameters.offset ?? 0;
      const page = this.shortcuts.slice(offset, offset + (parameters.limit ?? 6));
      return { offset, total: this.shortcuts.length, next: offset + page.length, shortcuts: page };
    }
    return {};
  }

  commands() {
    return this.requests.map((request) => request.command);
  }
}

async function makeApp(files) {
  const directory = await mkdtemp(join(tmpdir(), 'cyd-deploy-'));
  for (const [name, content] of Object.entries(files)) {
    const path = join(directory, name);
    if (content === null) await mkdir(path);
    else await writeFile(path, content);
  }
  return directory;
}

const noSleep = async () => {};

test('an app is every file in its directory, manifest first and entry second', async () => {
  const directory = await makeApp({
    'manifest.json': JSON.stringify({ id: 'weather', title: 'WEATHER' }),
    'sunny.jpg': Buffer.from([0xff, 0xd8, 0xff]),
    'main.py': 'import cyd\n',
    'README.md': '# not installed',
    '.DS_Store': 'x',
    'cache.pyc': 'x',
    __pycache__: null,
  });
  try {
    const app = await collectApp(directory);
    assert.equal(app.appId, 'weather');
    assert.equal(app.title, 'WEATHER');
    assert.deepEqual(app.files.map((file) => file.name), ['manifest.json', 'main.py', 'sunny.jpg']);
  } finally {
    await rm(directory, { recursive: true });
  }
});

test('the manifest id decides where a differently named directory installs', async () => {
  const directory = await makeApp({ 'manifest.json': '{"id":"pixelstorm","title":"PIXELSTORM"}', 'main.py': '' });
  try {
    assert.equal((await collectApp(directory)).appId, 'pixelstorm');
  } finally {
    await rm(directory, { recursive: true });
  }
});

test('things the device cannot store are refused before any upload', async () => {
  const cases = [
    [{ 'main.py': '', icons: null }, /icons\/: subdirectories cannot be installed/],
    [{ 'main.py': '', 'bad name.jpg': 'x' }, /bad name\.jpg: file names/],
    [{ 'main.py': '', 'a_very_long_asset_name.jpg': 'x' }, /file names must be 1-24/],
    [{ 'manifest.json': '{"title":"X","entry":"game.py"}', 'main.py': '' }, /game\.py: the entry file is missing/],
  ];
  for (const [files, message] of cases) {
    const directory = await makeApp(files);
    try {
      await assert.rejects(collectApp(directory), (error) => error instanceof DeployError && message.test(error.message));
    } finally {
      await rm(directory, { recursive: true });
    }
  }
});

test('a bad manifest is reported with the same field names as the device', async () => {
  const directory = await makeApp({ 'manifest.json': '{"tittle":"X"}', 'main.py': '' });
  try {
    await assert.rejects(collectApp(directory), (error) => error instanceof ManifestError && error.field === 'key');
  } finally {
    await rm(directory, { recursive: true });
  }
});

test('a binary file is sent in verified chunks that reassemble exactly', async () => {
  const bytes = Buffer.from(Array.from({ length: CHUNK_BYTES * 2 + 400 }, (_, index) => (index * 37) % 256));
  const client = new FakeClient();
  const outcome = await uploadFile(client, 'weather', { name: 'sunny.jpg', bytes });

  const [begin, ...rest] = client.requests;
  assert.equal(begin.command, 'desktop_package_replace_begin');
  assert.equal(begin.size, bytes.length);
  assert.equal(begin.sha256, createHash('sha256').update(bytes).digest('hex'));
  const chunks = rest.filter((request) => request.command === 'desktop_package_chunk');
  assert.deepEqual(chunks.map((chunk) => Buffer.from(chunk.data, 'base64').length), [CHUNK_BYTES, CHUNK_BYTES, 400]);
  assert.ok(chunks.every((chunk) => chunk.data.length <= 2400));
  assert.deepEqual(Buffer.concat(chunks.map((chunk) => Buffer.from(chunk.data, 'base64'))), bytes);
  assert.equal(client.commands().at(-1), 'desktop_package_finish');
  assert.equal(outcome.sha256, begin.sha256);
});

test('a file on disk streams the same way as bytes in memory', async () => {
  const bytes = Buffer.alloc(CHUNK_BYTES + 1, 7);
  const directory = await makeApp({ 'movie.mjp': bytes });
  try {
    const fromMemory = new FakeClient();
    const fromDisk = new FakeClient();
    await uploadFile(fromMemory, 'mjpplayer', { name: 'movie.mjp', bytes });
    await uploadFile(fromDisk, 'mjpplayer', { name: 'movie.mjp', path: join(directory, 'movie.mjp'), size: bytes.length });
    assert.deepEqual(fromDisk.requests, fromMemory.requests);
  } finally {
    await rm(directory, { recursive: true });
  }
});

test('no-replace keeps the device\'s refusal to overwrite, and empty files need no chunk', async () => {
  const client = new FakeClient();
  await uploadFile(client, 'notes', { name: 'empty.txt', bytes: Buffer.alloc(0) }, { replace: false });
  assert.deepEqual(client.commands(), ['desktop_package_begin', 'desktop_package_finish']);
});

test('deploy leaves the running app first and waits for the runtime to let go', async () => {
  const directory = await makeApp({ 'main.py': 'import cyd\n' });
  try {
    const client = new FakeClient({ attachedPolls: 2 });
    await deployApp(client, directory, { sleep: noSleep });
    const commands = client.commands();
    assert.equal(commands[0], 'desktop_home');
    assert.deepEqual(commands.slice(1, 4), ['desktop_diagnostics', 'desktop_diagnostics', 'desktop_diagnostics']);
    assert.equal(commands[4], 'desktop_package_replace_begin');
  } finally {
    await rm(directory, { recursive: true });
  }
});

test('pinning reuses the app\'s existing shortcut instead of adding a second icon', async () => {
  const legacy = new FakeClient({ shortcuts: [{ shortcut_id: 'pyconsole', app_id: 'console', title: 'OLD' }] });
  assert.deepEqual(await pinApp(legacy, 'console', 'CONSOLE'), { shortcut_id: 'pyconsole', created: false });
  assert.deepEqual(legacy.requests.at(-1),
    { command: 'desktop_shortcut_update', shortcut_id: 'pyconsole', app_id: 'console', title: 'CONSOLE' });

  const fresh = new FakeClient();
  assert.deepEqual(await pinApp(fresh, 'weather', 'WEATHER'), { shortcut_id: 'weather', created: true });
  assert.equal(fresh.requests.at(-1).command, 'desktop_shortcut_create');
});

test('the MCP app root cannot be escaped', async () => {
  const root = await makeApp({ weather: null });
  const outside = await makeApp({ 'secret.txt': 'x' });
  try {
    assert.equal(await resolveInside(root, 'weather'), await resolveInside(root, join(root, 'weather')));
    await assert.rejects(resolveInside(root, '..'), DeployError);
    await assert.rejects(resolveInside(root, outside), DeployError);
    await assert.rejects(resolveInside(root, '.'), DeployError);
    try {
      await symlink(outside, join(root, 'link'), 'junction');
      await assert.rejects(resolveInside(root, 'link'), DeployError);
    } catch (error) {
      if (error.code !== 'EPERM') throw error;  // symlinks may need privileges on Windows
    }
  } finally {
    await rm(root, { recursive: true });
    await rm(outside, { recursive: true });
  }
});
