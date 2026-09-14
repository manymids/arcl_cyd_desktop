import { DesktopClient } from './desktop-client.mjs';

const client = new DesktopClient({ timeoutMs: 15000 });
const sleep = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

async function allNodes() {
  const nodes = [];
  let offset = 0;
  for (;;) {
    const page = await client.request('desktop_ui_tree', { offset, limit: 6 });
    nodes.push(...page.nodes);
    if (page.next >= page.total || page.next <= offset) return nodes;
    offset = page.next;
  }
}

try {
  await client.request('desktop_home');
  const before = await client.request('desktop_diagnostics');
  const launched = await client.request('desktop_launch', { app_id: 'neon3d' });
  if (launched.view !== 'native' || launched.foreground !== 'neon3d')
    throw new Error(`native launch failed: ${JSON.stringify(launched)}`);
  await sleep(2200);
  const nodes = await allNodes();
  for (const id of ['native.window', 'native.turn_left', 'native.forward', 'native.turn_right', 'native.fire']) {
    if (!nodes.some((node) => node.id === id)) throw new Error(`missing native UI node ${id}`);
  }
  await client.request('desktop_tap', { x: 30, y: 200 });
  await client.request('desktop_tap', { x: 105, y: 200 });
  await client.request('desktop_tap', { x: 270, y: 200 });
  await sleep(1000);
  const input = await client.request('desktop_input_read');
  const after = await client.request('desktop_diagnostics');
  const nativeFrames = after.native_frames;
  if (nativeFrames < 8) throw new Error(`native renderer stalled at ${nativeFrames} frames`);
  if (input.events?.[0] !== 'tap.native.fire') throw new Error(`fire input missing: ${JSON.stringify(input)}`);
  const result = {
    version: launched.version,
    native_frames: nativeFrames,
    observed_fps: Number((nativeFrames / 3.2).toFixed(2)),
    native_last_frame_ms: after.native_last_frame_ms,
    native_average_frame_ms: after.native_average_frame_ms,
    heap_free: after.heap_free,
    heap_min: after.heap_min,
    spi_transactions: after.spi_transactions - before.spi_transactions,
    spi_bytes: after.spi_bytes - before.spi_bytes,
  };
  console.log(JSON.stringify(result));
} finally {
  try { await client.request('desktop_home'); } catch {}
  await client.close();
}
