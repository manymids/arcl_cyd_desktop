// Install an app directory, or add one asset to an installed app.
//
//   node bridge/deploy.mjs <app-dir> [--pin] [--launch] [--no-replace] [--port COM6]
//   node bridge/deploy.mjs --app <app-id> --asset <file> [--fast] [--no-replace] [--port COM6]
//
// This replaces the per-app install-*.mjs scripts, each of which carried its
// own copy of the upload loop with its own chunk size and its own idea of which
// files an app consists of. Here an app is simply its directory: every file in
// it is uploaded, manifest.json is checked against the same schema the device
// uses before anything is sent, and binary assets travel the same way as code.
// The MCP bridge (index.mjs) uses these functions too.

import { createHash } from 'node:crypto';
import { createReadStream } from 'node:fs';
import { open, readdir, readFile, realpath, stat } from 'node:fs/promises';
import { basename, isAbsolute, join, relative, resolve, sep } from 'node:path';
import { pathToFileURL } from 'node:url';
import { isSafeComponent, parseManifest, validateManifest } from './manifest.mjs';

// 1800 bytes is 2400 Base64 characters, the largest chunk that fits the
// device's 2500-byte payload buffer together with the JSONL framing.
export const CHUNK_BYTES = 1800;

const SKIPPED_FILES = [/^README(\..*)?$/i, /\.pyc$/i, /^\./];
const SKIPPED_DIRECTORIES = new Set(['__pycache__']);

export class DeployError extends Error {}

const defaultSleep = (milliseconds) => new Promise((done) => setTimeout(done, milliseconds));

// Upload order: manifest.json first so the title is right the moment the entry
// arrives (the device rescans its app list after every file), then the entry so
// the app becomes launchable, then assets.
function uploadRank(name, entry) {
  if (name === 'manifest.json') return 0;
  if (name === entry) return 1;
  return 2;
}

export async function collectApp(directory) {
  const entries = await readdir(directory, { withFileTypes: true });
  const files = [];
  let manifestText = null;

  for (const entry of entries) {
    if (entry.isDirectory()) {
      if (SKIPPED_DIRECTORIES.has(entry.name) || entry.name.startsWith('.')) continue;
      throw new DeployError(
        `${entry.name}/: subdirectories cannot be installed; an app on the device is one flat directory`);
    }
    if (!entry.isFile() || SKIPPED_FILES.some((pattern) => pattern.test(entry.name))) continue;
    if (!isSafeComponent(entry.name)) {
      throw new DeployError(`${entry.name}: file names must be 1-24 of A-Z a-z 0-9 _ . -`);
    }
    const path = join(directory, entry.name);
    const { size } = await stat(path);
    if (entry.name === 'manifest.json') manifestText = await readFile(path, 'utf8');
    files.push({ name: entry.name, path, size });
  }

  // The device names an app by its directory. A local directory may be named
  // differently (examples/pixel_barrage installs as pixelstorm), so manifest.json
  // can say which id to install under.
  let appId = basename(resolve(directory));
  if (manifestText !== null) {
    try {
      const data = JSON.parse(manifestText);
      if (data && typeof data.id === 'string') appId = data.id;
    } catch {
      // parseManifest below reports the syntax error with its field.
    }
  }
  if (!isSafeComponent(appId)) {
    throw new DeployError(`app id "${appId}" must be 1-24 of A-Z a-z 0-9 _ . -; set "id" in manifest.json`);
  }

  const { title, entry } = manifestText === null
    ? validateManifest(appId, null)
    : parseManifest(appId, manifestText);
  if (!files.some((file) => file.name === entry)) {
    throw new DeployError(`${entry}: the entry file is missing from ${directory}`);
  }

  files.sort((a, b) => uploadRank(a.name, entry) - uploadRank(b.name, entry) || a.name.localeCompare(b.name));
  return { appId, title, entry, files };
}

async function sha256Of(file) {
  if (file.bytes) return createHash('sha256').update(file.bytes).digest('hex');
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(file.path)) hash.update(chunk);
  return hash.digest('hex');
}

// Streams a file into /sd/apps/<appId>/<name>. `file` is either { name, bytes }
// or { name, path, size }; a path is read chunk by chunk so a multi-megabyte
// movie never sits in memory. With replace, an existing file is swapped only
// after the new one has arrived and verified.
export async function uploadFile(client, appId, file, { replace = true, onProgress } = {}) {
  if (!isSafeComponent(appId)) throw new DeployError(`app id "${appId}" is not a safe name`);
  if (!isSafeComponent(file.name)) throw new DeployError(`${file.name}: not a safe file name`);
  const size = file.bytes ? file.bytes.length : file.size;
  const sha256 = await sha256Of(file);

  await client.request(replace ? 'desktop_package_replace_begin' : 'desktop_package_begin', {
    app_id: appId, file: file.name, size, sha256,
  });

  if (file.bytes) {
    for (let offset = 0; offset < size; offset += CHUNK_BYTES) {
      await client.request('desktop_package_chunk', {
        data: file.bytes.subarray(offset, offset + CHUNK_BYTES).toString('base64'),
      });
      onProgress?.(Math.min(offset + CHUNK_BYTES, size), size);
    }
  } else {
    const handle = await open(file.path, 'r');
    const buffer = Buffer.alloc(CHUNK_BYTES);
    try {
      for (let offset = 0; offset < size;) {
        const { bytesRead } = await handle.read(buffer, 0, Math.min(CHUNK_BYTES, size - offset), offset);
        if (!bytesRead) throw new DeployError(`${file.name}: file ended at ${offset} of ${size} bytes`);
        await client.request('desktop_package_chunk', { data: buffer.subarray(0, bytesRead).toString('base64') });
        offset += bytesRead;
        onProgress?.(offset, size);
      }
    } finally {
      await handle.close();
    }
  }

  await client.request('desktop_package_finish');
  return { file: file.name, bytes: size, sha256 };
}

// The device refuses package operations while a foreground app holds the
// runtime, so leave it and wait until the runtime has let go.
export async function returnHome(client, { timeoutMs = 10000, sleep = defaultSleep } = {}) {
  await client.request('desktop_home');
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const diagnostics = await client.request('desktop_diagnostics');
    if (!diagnostics?.runtime_watchdog_attached) return;
    if (Date.now() >= deadline) {
      throw new DeployError('the foreground app did not stop; the device refuses uploads while an app runs');
    }
    await sleep(200);
  }
}

async function listShortcuts(client) {
  const shortcuts = [];
  for (let offset = 0; ;) {
    const page = await client.request('desktop_shortcuts_list', { offset, limit: 6 });
    shortcuts.push(...(page?.shortcuts ?? []));
    if (!page || page.next >= page.total || page.next <= offset) return shortcuts;
    offset = page.next;
  }
}

// Pins an app to Home. An existing shortcut for the same app is reused whatever
// its id - the old install scripts created ids such as "pyconsole" - so pinning
// again never leaves two icons for one app.
export async function pinApp(client, appId, title) {
  const existing = (await listShortcuts(client)).find((shortcut) => shortcut.app_id === appId);
  if (existing) {
    if (existing.title !== title) {
      await client.request('desktop_shortcut_update', { shortcut_id: existing.shortcut_id, app_id: appId, title });
    }
    return { shortcut_id: existing.shortcut_id, created: false };
  }
  await client.request('desktop_shortcut_create', { shortcut_id: appId, app_id: appId, title });
  return { shortcut_id: appId, created: true };
}

export async function deployApp(client, directory, { replace = true, pin = false, launch = false, log = () => {}, sleep } = {}) {
  const app = await collectApp(directory);
  await returnHome(client, { sleep });
  const files = [];
  for (const file of app.files) {
    log(`upload ${app.appId}/${file.name} (${file.size} bytes)`);
    files.push(await uploadFile(client, app.appId, file, { replace }));
  }
  const outcome = { app_id: app.appId, title: app.title, entry: app.entry, files };
  if (pin) outcome.shortcut = await pinApp(client, app.appId, app.title);
  if (launch) outcome.launched = await client.request('desktop_script_launch', { app_id: app.appId });
  return outcome;
}

export async function deployAsset(client, appId, path, { replace = true, fast = false, onProgress, sleep } = {}) {
  const name = basename(path);
  const { size } = await stat(path);
  await returnHome(client, { sleep });
  if (fast) await client.setBaudRate(921600);
  try {
    return await uploadFile(client, appId, { name, path, size }, { replace, onProgress });
  } finally {
    if (fast) await client.setBaudRate(115200);
  }
}

// Resolves `requested` inside `root`, following symlinks, and refuses anything
// that lands outside it. The MCP bridge must not become a way to read arbitrary
// files on the PC.
export async function resolveInside(root, requested) {
  const base = await realpath(root);
  const target = await realpath(isAbsolute(requested) ? requested : join(base, requested));
  const path = relative(base, target);
  if (path === '' || path === '..' || path.startsWith(`..${sep}`) || isAbsolute(path)) {
    throw new DeployError(`${requested}: not an app directory inside ${base}`);
  }
  return target;
}

function parseArguments(argv) {
  const options = { replace: true, pin: false, launch: false, fast: false, port: process.env.CYD_DESKTOP_PORT ?? null };
  const positional = [];
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === '--pin') options.pin = true;
    else if (argument === '--launch') options.launch = true;
    else if (argument === '--no-replace') options.replace = false;
    else if (argument === '--fast') options.fast = true;
    else if (argument === '--port') options.port = argv[++index];
    else if (argument === '--app') options.app = argv[++index];
    else if (argument === '--asset') options.asset = argv[++index];
    else if (argument.startsWith('--')) throw new DeployError(`unknown option ${argument}`);
    else positional.push(argument);
  }
  if (options.asset ? !options.app || positional.length : positional.length !== 1) {
    throw new DeployError([
      'usage: node bridge/deploy.mjs <app-dir> [--pin] [--launch] [--no-replace] [--port COM6]',
      '       node bridge/deploy.mjs --app <app-id> --asset <file> [--fast] [--no-replace] [--port COM6]',
    ].join('\n'));
  }
  options.directory = positional[0];
  return options;
}

async function main() {
  const options = parseArguments(process.argv.slice(2));
  const { DesktopClient } = await import('./desktop-client.mjs');
  const client = new DesktopClient({ path: options.port, timeoutMs: options.asset ? 180000 : 15000 });
  try {
    let outcome;
    if (options.asset) {
      let nextPercent = 10;
      outcome = await deployAsset(client, options.app, options.asset, {
        replace: options.replace,
        fast: options.fast,
        onProgress: (done, total) => {
          const percent = Math.floor((done * 100) / total);
          if (percent >= nextPercent) {
            process.stderr.write(`${basename(options.asset)} ${percent}% (${done}/${total})\n`);
            nextPercent += 10;
          }
        },
      });
    } else {
      outcome = await deployApp(client, options.directory, {
        replace: options.replace,
        pin: options.pin,
        launch: options.launch,
        log: (line) => process.stderr.write(`${line}\n`),
      });
    }
    console.log(JSON.stringify(outcome, null, 2));
  } finally {
    await client.close();
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  main().catch((error) => {
    process.stderr.write(`${error.message}\n`);
    process.exitCode = 1;
  });
}
