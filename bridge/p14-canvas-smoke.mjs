import { DesktopClient } from './desktop-client.mjs';
import { readFileSync } from 'node:fs';

const EXPECTED_VERSION = JSON.parse(readFileSync(new URL('../package.json', import.meta.url), 'utf8')).version;

const client = new DesktopClient({ timeoutMs: 12000 });
const sleep = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

async function all(command, field, limit = 4) {
  const values = [];
  let offset = 0;
  let first;
  for (;;) {
    const page = await client.request(command, { offset, limit });
    first ??= page;
    values.push(...page[field]);
    if (page.next >= page.total || page.next <= offset) return { ...first, [field]: values };
    offset = page.next;
  }
}

async function waitRuntime(predicate, timeoutMs = 6000) {
  const deadline = Date.now() + timeoutMs;
  let state;
  do {
    state = await client.request('desktop_runtime_status');
    if (predicate(state)) return state;
    await sleep(80);
  } while (Date.now() < deadline);
  throw new Error(`runtime condition timed out: ${JSON.stringify(state)}`);
}

try {
  const status = await client.request('desktop_status');
  if (status.version !== EXPECTED_VERSION) throw new Error(`firmware ${status.version}, expected ${EXPECTED_VERSION}`);
  if (status.foreground !== 'canvasfx') await client.request('desktop_script_launch', { app_id: 'canvasfx' });
  await waitRuntime((state) => state.attached && state.frame >= 1, 12000);
  await sleep(250);

  const tree = await all('desktop_ui_tree', 'nodes');
  const canvas = tree.nodes.find((node) => node.id === 'app.canvas');
  if (!canvas || canvas.role !== 'canvas' || canvas.label !== '48 primitives') {
    const logs = await all('desktop_logs', 'events');
    const runtime = await client.request('desktop_runtime_status');
    throw new Error(`Canvas semantic node missing: ${JSON.stringify({ canvas, runtime, logs: logs.events })}`);
  }

  await client.request('desktop_pause');
  const paused = await waitRuntime((state) => state.paused && state.pending_steps === 0);
  await sleep(220);
  const held = await client.request('desktop_runtime_status');
  if (held.frame !== paused.frame) throw new Error('paused Canvas frame did not hold');
  await client.request('desktop_step');
  const stepped = await waitRuntime((state) => state.frame === held.frame + 1 && state.pending_steps === 0);
  await client.request('desktop_run', { frames: 3 });
  const bounded = await waitRuntime((state) => state.frame === stepped.frame + 3 && state.pending_steps === 0);
  await client.request('desktop_resume');

  await client.request('desktop_tap', { x: 62, y: 180 });
  const touchDeadline = Date.now() + 8000;
  let mode;
  do {
    await sleep(250);
    const changedTree = await all('desktop_ui_tree', 'nodes');
    mode = changedTree.nodes.find((node) => node.id === 'app.button.mode');
  } while (mode?.label !== 'LASER' && Date.now() < touchDeadline);
  if (!mode || mode.label !== 'LASER') throw new Error(`Canvas touch callback failed: ${JSON.stringify(mode)}`);

  const perfStart = await client.request('desktop_runtime_status');
  const perfStartedAt = Date.now();
  await sleep(3000);
  const perfEnd = await client.request('desktop_runtime_status');
  const perfSeconds = (Date.now() - perfStartedAt) / 1000;
  const measuredFps = (perfEnd.frame - perfStart.frame) / perfSeconds;
  if (measuredFps < 1) throw new Error(`Canvas animation too slow: ${measuredFps.toFixed(2)} fps`);

  const diagnostics = await client.request('desktop_diagnostics');
  console.log(JSON.stringify({ status, canvas, runtime: {
    paused: paused.frame, stepped: stepped.frame, bounded: bounded.frame,
  }, mode, performance: { frames: perfEnd.frame - perfStart.frame, seconds: perfSeconds, fps: measuredFps }, diagnostics }));
} finally {
  try { await client.request('desktop_home'); } catch {}
  await client.close();
}
