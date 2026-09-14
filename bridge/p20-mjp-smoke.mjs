import { DesktopClient } from './desktop-client.mjs';
import { readFileSync } from 'node:fs';

const EXPECTED_VERSION = JSON.parse(readFileSync(new URL('../package.json', import.meta.url), 'utf8')).version;

const client = new DesktopClient({ timeoutMs: 15000 });

async function allNodes() {
  const nodes = [];
  let offset = 0;
  for (;;) {
    const page = await client.request('desktop_ui_tree', { offset, limit: 4 });
    nodes.push(...page.nodes);
    if (page.next >= page.total || page.next <= offset) return nodes;
    offset = page.next;
  }
}

try {
  await client.request('desktop_home');
  await client.request('desktop_script_launch', { app_id: 'mjpplayer' });
  const pickerDeadline = Date.now() + 10000;
  let movieButton;
  do {
    await new Promise((resolve) => setTimeout(resolve, 250));
    movieButton = (await allNodes()).find((node) => node.id === 'app.button.movie0');
  } while (!movieButton && Date.now() < pickerDeadline);
  if (!movieButton) throw new Error('movie picker did not expose a selectable file');
  await client.request('desktop_tap', {
    x: movieButton.x + Math.floor(movieButton.width / 2),
    y: movieButton.y + Math.floor(movieButton.height / 2),
  });
  await new Promise((resolve) => setTimeout(resolve, 2500));
  const status = await client.request('desktop_status');
  const before = await client.request('desktop_diagnostics');
  await new Promise((resolve) => setTimeout(resolve, 3000));
  const after = await client.request('desktop_diagnostics');
  if (status.version !== EXPECTED_VERSION) throw new Error(`firmware ${status.version}, expected ${EXPECTED_VERSION}`);
  if (status.view !== 'native' || status.foreground !== 'mjpplayer')
    throw new Error(`movie did not launch: ${JSON.stringify(status)}`);
  if (!after.runtime_watchdog_attached) throw new Error('movie feeder runtime is not attached');
  if (after.native_frames <= before.native_frames)
    throw new Error(`movie frames did not advance: ${before.native_frames} -> ${after.native_frames}`);
  console.log(JSON.stringify({ movieButton, status, before, after, advanced: after.native_frames - before.native_frames }));
} finally {
  await client.close();
}
