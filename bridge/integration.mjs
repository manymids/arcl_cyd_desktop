import { DesktopClient } from './desktop-client.mjs';

const client = new DesktopClient();
try {
  const status = await client.request('desktop_status');
  const clock = await client.request('desktop_launch', { app_id: 'clock' });
  const tree = await client.request('desktop_ui_tree');
  const sd = await client.request('desktop_sd_status');
  const home = await client.request('desktop_home');
  if (clock.view !== 'clock' || tree.view !== 'clock' || home.view !== 'home' || typeof sd.mounted !== 'boolean') throw new Error('unexpected desktop transition');
  console.log(JSON.stringify({ status, clock, tree, sd, home }));
} finally {
  await client.close();
}
