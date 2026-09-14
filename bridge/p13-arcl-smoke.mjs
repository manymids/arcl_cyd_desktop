import { createHash } from 'node:crypto';
import { DesktopClient } from './desktop-client.mjs';

const client = new DesktopClient({ timeoutMs: 12000 });
const sleep = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

async function all(command, field, limit) {
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

async function upload(appId, source) {
  const bytes = Buffer.from(source);
  const sha256 = createHash('sha256').update(bytes).digest('hex');
  await client.request('desktop_package_begin', { app_id: appId, file: 'main.py', size: bytes.length, sha256 });
  for (let offset = 0; offset < bytes.length; offset += 360)
    await client.request('desktop_package_chunk', { data: bytes.subarray(offset, offset + 360).toString('base64') });
  return client.request('desktop_package_finish');
}

async function waitForRuntime(predicate, timeoutMs = 6000) {
  const deadline = Date.now() + timeoutMs;
  let state;
  do {
    state = await client.request('desktop_runtime_status');
    if (predicate(state)) return state;
    await sleep(100);
  } while (Date.now() < deadline);
  throw new Error(`runtime condition timed out: ${JSON.stringify(state)}`);
}

const checkId = 'p13check';
const checkSource = `import time
import cyd
state = cyd.restore({"launches": 0})
state["launches"] += 1
cyd.checkpoint(state)
cyd.clear()
cyd.title("P13 CHECK")
cyd.text(20, 85, "CHECKPOINT %d" % state["launches"])
cyd.log("checkpoint:%d" % state["launches"])
while cyd.update():
    time.sleep_ms(50)
`;

try {
  console.error('[p13] settle and home');
  await sleep(500);
  await client.request('desktop_home');
  await sleep(200);

  const homeTree = await all('desktop_ui_tree', 'nodes', 4);
  if (!homeTree.nodes.some((node) => node.id === 'taskbar.start')) throw new Error('taskbar missing from retained tree');
  await client.request('desktop_tap', { x: 275, y: 90 });
  await sleep(1200);
  const tapped = await client.request('desktop_status');
  if (tapped.view !== 'settings') throw new Error(`synthetic tap opened ${tapped.view}, not settings`);
  const input = await client.request('desktop_input_state');
  const settings = await client.request('desktop_settings_get');
  const settingsRoundTrip = await client.request('desktop_settings_set', settings);
  console.error('[p13] input and settings passed');
  await client.request('desktop_home');
  await sleep(180);

  const appsBefore = await all('desktop_installed_apps_list', 'apps', 6);
  if (appsBefore.apps.some((app) => app.app_id === checkId)) await client.request('desktop_app_delete', { app_id: checkId });
  await upload(checkId, checkSource);
  console.error('[p13] checkpoint package uploaded');
  for (let launch = 1; launch <= 2; launch += 1) {
    await client.request('desktop_script_launch', { app_id: checkId });
    await sleep(1200);
    const logs = await all('desktop_logs', 'events', 4);
    if (!logs.events.some((event) => event.value === `checkpoint:${launch}`))
      throw new Error(`checkpoint launch ${launch} was not observed`);
    await client.request('desktop_home');
    console.error(`[p13] checkpoint launch ${launch} passed`);
    await sleep(220);
  }
  await client.request('desktop_app_delete', { app_id: checkId });

  console.error('[p13] cooperative runtime');
  await client.request('desktop_script_launch', { app_id: 'neonpulse' });
  await waitForRuntime((state) => state.attached);
  console.error('[p13] runtime attached');
  await client.request('desktop_pause');
  const paused1 = await waitForRuntime((state) => state.attached && state.paused && state.pending_steps === 0);
  await sleep(250);
  const paused2 = await client.request('desktop_runtime_status');
  if (!paused2.attached || !paused2.paused || paused2.frame !== paused1.frame) throw new Error('runtime did not hold at pause');
  console.error('[p13] pause held');
  await client.request('desktop_step');
  const stepped = await waitForRuntime((state) => state.frame === paused2.frame + 1 && state.pending_steps === 0);
  if (stepped.frame !== paused2.frame + 1) throw new Error('runtime step did not advance exactly once');
  console.error('[p13] one step passed');
  const granted = await client.request('desktop_run', { frames: 2 });
  const run = await waitForRuntime((state) => state.frame === stepped.frame + 2 && state.pending_steps === 0);
  if (run.frame !== stepped.frame + 2 || !run.paused) throw new Error('bounded run did not advance two frames');
  console.error('[p13] bounded run passed');
  await client.request('desktop_resume');
  console.error('[p13] resumed');
  await sleep(180);
  console.error('[p13] returning home');
  await client.request('desktop_home');
  console.error('[p13] runtime passed');
  await sleep(180);

  const diagnostics = await client.request('desktop_diagnostics');
  const appsAfter = await all('desktop_installed_apps_list', 'apps', 6);
  if (appsAfter.apps.some((app) => app.app_id === checkId)) throw new Error('temporary checkpoint app was not deleted');
  console.log(JSON.stringify({
    home_nodes: homeTree.nodes.length,
    synthetic_tap: { view: tapped.view, input },
    settings: settingsRoundTrip,
    installed_apps: appsAfter.apps.length,
    checkpoint_restore: true,
    runtime: { paused_frame: paused2.frame, stepped_frame: stepped.frame, run_frame: run.frame, granted },
    diagnostics,
  }));
} finally {
  try { await client.request('desktop_home'); } catch {}
  await client.close();
}
