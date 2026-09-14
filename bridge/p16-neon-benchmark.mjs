import { DesktopClient } from './desktop-client.mjs';

const client = new DesktopClient({ timeoutMs: 15000 });
const sleep = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

async function waitRuntime(predicate, timeoutMs = 15000) {
  const deadline = Date.now() + timeoutMs;
  let state;
  do {
    state = await client.request('desktop_runtime_status');
    if (predicate(state)) return state;
    await sleep(60);
  } while (Date.now() < deadline);
  throw new Error(`runtime condition timed out: ${JSON.stringify(state)}`);
}

try {
  await client.request('desktop_home');
  await client.request('desktop_script_launch', { app_id: 'neonpulse' });
  await waitRuntime((state) => state.attached && state.frame >= 2);
  await client.request('desktop_pause');
  const paused = await waitRuntime((state) => state.paused && state.pending_steps === 0);
  const before = await client.request('desktop_diagnostics');
  const started = Date.now();
  await client.request('desktop_run', { frames: 5 });
  const finished = await waitRuntime((state) =>
    state.paused && state.pending_steps === 0 && state.frame === paused.frame + 5);
  const elapsed_ms = Date.now() - started;
  const after = await client.request('desktop_diagnostics');
  console.log(JSON.stringify({
    version: (await client.request('desktop_status')).version,
    start_frame: paused.frame,
    end_frame: finished.frame,
    elapsed_ms,
    spi_transactions: after.spi_transactions - before.spi_transactions,
    spi_bytes: after.spi_bytes - before.spi_bytes,
    renders: after.render_count - before.render_count,
    heap_free: after.heap_free,
    heap_min: after.heap_min,
  }));
} finally {
  try { await client.request('desktop_home'); } catch {}
  await client.close();
}
