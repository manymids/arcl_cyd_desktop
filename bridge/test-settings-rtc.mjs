// Hardware check for Settings > DATE & TIME: raise the minute five times, save,
// and confirm the Settings row shows the new time.
//
// Taps are aimed at UI tree nodes by id, not fixed coordinates, so a layout
// change (such as the sixth Settings row) cannot quietly send them elsewhere.
// Changes the board's clock by five minutes.

import { DesktopClient } from './desktop-client.mjs';

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function nodes(client) {
  const all = [];
  for (let offset = 0; ;) {
    const page = await client.request('desktop_ui_tree', { offset, limit: 4 });
    all.push(...(page.nodes ?? []));
    if (page.next >= page.total || !page.nodes?.length) return all;
    offset = page.next;
  }
}

async function tapNode(client, id) {
  const node = (await nodes(client)).find((candidate) => candidate.id === id);
  if (!node) throw new Error(`UI node ${id} not found`);
  await client.request('desktop_tap', { x: node.x + Math.floor(node.width / 2), y: node.y + Math.floor(node.height / 2) });
  await sleep(400);
  return node;
}

const client = new DesktopClient();
await client.open();
try {
  await client.request('desktop_home');
  await sleep(400);
  await client.request('desktop_launch', { app_id: 'settings' });
  await sleep(600);

  const before = await tapNode(client, 'settings.rtc');
  const [hour, minute] = before.label.split(':').map(Number);
  if (Number.isNaN(hour) || Number.isNaN(minute)) throw new Error(`unexpected time label ${before.label}`);
  for (let press = 0; press < 5; press += 1) await tapNode(client, 'settings.rtc.min_up');
  await tapNode(client, 'settings.rtc.save');

  const after = (await nodes(client)).find((node) => node.id === 'settings.rtc');
  if (!after) throw new Error('Settings did not return from DATE & TIME');
  const expected = `${String(hour).padStart(2, '0')}:${String((minute + 5) % 60).padStart(2, '0')}`;
  // The clock may tick over a minute while the test runs.
  const [, afterMinute] = after.label.split(':').map(Number);
  const moved = (afterMinute - minute + 60) % 60;
  if (moved < 5 || moved > 6) throw new Error(`time ${before.label} -> ${after.label}, expected about ${expected}`);
  console.log(`RTC set: ${before.label} -> ${after.label}`);
} finally {
  await client.request('desktop_home').catch(() => {});
  await client.close();
}
