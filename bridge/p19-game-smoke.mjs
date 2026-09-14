import { DesktopClient } from './desktop-client.mjs';

const client = new DesktopClient({ timeoutMs: 15000 });
const sleep = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

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
  await client.request('desktop_script_launch', { app_id: 'pixelstorm' });
  await sleep(1400);
  const before = await client.request('desktop_diagnostics');
  await sleep(3200);
  const after = await client.request('desktop_diagnostics');
  const runtime = await client.request('desktop_runtime_status');
  const game = (await allNodes()).find((node) => node.id === 'app.game');
  const frames = after.render_count - before.render_count;
  if (!runtime.attached) throw new Error('Pixel Storm runtime is not attached');
  if (!game) throw new Error('native game surface is missing');
  if (frames < 25) throw new Error(`game renderer stalled at ${frames} frames`);
  console.log(JSON.stringify({
    version: (await client.request('desktop_status')).version,
    game,
    runtime_frame: runtime.frame,
    frames,
    fps: frames / 3.2,
    heap_free: after.heap_free,
    heap_min: after.heap_min,
    spi_bytes: after.spi_bytes - before.spi_bytes,
  }));
} finally {
  await client.close();
}
