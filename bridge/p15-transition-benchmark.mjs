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

async function waitNode(id, label = null, timeoutMs = 15000) {
  const deadline = Date.now() + timeoutMs;
  let nodes = [];
  do {
    nodes = await allNodes();
    const node = nodes.find((item) => item.id === id && (label === null || item.label === label));
    if (node) return node;
    await sleep(40);
  } while (Date.now() < deadline);
  throw new Error(`timed out waiting for ${id}: ${JSON.stringify(nodes)}`);
}

async function measure(label, action, nodeId, nodeLabel = null) {
  const before = await client.request('desktop_diagnostics');
  const started = Date.now();
  await action();
  await waitNode(nodeId, nodeLabel);
  const elapsed_ms = Date.now() - started;
  const after = await client.request('desktop_diagnostics');
  return {
    label,
    elapsed_ms,
    renders: after.render_count - before.render_count,
    spi_transactions: after.spi_transactions - before.spi_transactions,
    spi_bytes: after.spi_bytes - before.spi_bytes,
    heap_free: after.heap_free,
    heap_min: after.heap_min,
  };
}

try {
  await client.request('desktop_home');
  await waitNode('home.clock');
  const results = [];
  results.push(await measure('home_to_clock',
    () => client.request('desktop_launch', { app_id: 'clock' }), 'clock.face'));
  results.push(await measure('clock_to_home',
    () => client.request('desktop_home'), 'home.clock'));
  results.push(await measure('home_to_canvas',
    () => client.request('desktop_script_launch', { app_id: 'canvasfx' }),
    'app.canvas', '48 primitives'));
  console.log(JSON.stringify({ version: (await client.request('desktop_status')).version, results }));
} finally {
  try { await client.request('desktop_home'); } catch {}
  await client.close();
}
