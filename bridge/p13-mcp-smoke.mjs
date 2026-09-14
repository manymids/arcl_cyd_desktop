import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';

const transport = new StdioClientTransport({
  command: process.execPath,
  args: ['bridge/index.mjs'],
  cwd: process.cwd(),
  stderr: 'pipe',
});
const client = new Client({ name: 'cyd-p13-mcp-smoke', version: '0.0.1' }, { capabilities: {} });

function textValue(response) {
  if (response.isError) throw new Error(response.content?.[0]?.text ?? 'MCP tool failed');
  const item = response.content?.find((entry) => entry.type === 'text');
  if (!item) throw new Error('MCP tool returned no text');
  return JSON.parse(item.text);
}

async function call(name, arguments_ = {}) {
  return textValue(await client.callTool({ name, arguments: arguments_ }));
}

try {
  await client.connect(transport);
  const listed = await client.listTools();
  const expected = [
    'arcl_status', 'cyd_ui_tree', 'cyd_screen_mirror',
    'cyd_tap', 'cyd_input_state', 'cyd_input_macro',
    'cyd_installed_apps_list', 'cyd_settings_get', 'cyd_settings_set',
    'cyd_logs', 'cyd_diagnostics', 'cyd_pause', 'cyd_step',
    'cyd_run', 'cyd_resume', 'cyd_app_delete',
  ];
  const names = listed.tools.map((tool) => tool.name);
  const missing = expected.filter((name) => !names.includes(name));
  if (missing.length) throw new Error(`missing MCP tools: ${missing.join(', ')}`);

  await call('cyd_home');
  const status = await call('arcl_status');
  const tree = await call('cyd_ui_tree');
  const apps = await call('cyd_installed_apps_list');
  const settings = await call('cyd_settings_get');
  const input = await call('cyd_input_state');
  const diagnostics = await call('cyd_diagnostics');
  const logs = await call('cyd_logs');

  const mirrorResponse = await client.callTool({ name: 'cyd_screen_mirror', arguments: {} });
  if (mirrorResponse.isError) throw new Error('screen mirror MCP call failed');
  const imageItem = mirrorResponse.content?.find((entry) => entry.type === 'image');
  const png = imageItem ? Buffer.from(imageItem.data, 'base64') : Buffer.alloc(0);
  if (imageItem?.mimeType !== 'image/png' || png.subarray(0, 8).toString('hex') !== '89504e470d0a1a0a'
      || png.readUInt32BE(16) !== 320 || png.readUInt32BE(20) !== 240) {
    throw new Error('screen mirror did not return a valid 320x240 PNG');
  }

  await call('cyd_tap', { x: 275, y: 90 });
  await new Promise((resolve) => setTimeout(resolve, 1200));
  const tapped = await call('arcl_status');
  if (tapped.view !== 'settings') throw new Error(`MCP tap reached ${tapped.view}, not settings`);
  const returned = await call('cyd_home');

  console.log(JSON.stringify({
    advertised_tools: names.length,
    required_tools: expected.length,
    missing,
    status,
    ui_nodes: tree.nodes.length,
    installed_apps: apps.apps.length,
    settings,
    input,
    diagnostics,
    retained_events: logs.events.length,
    mirror: { mime_type: imageItem.mimeType, bytes: png.length, width: 320, height: 240 },
    tap_view: tapped.view,
    final_view: returned.view,
  }));
} finally {
  await client.close();
}

