import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { fileURLToPath } from 'node:url';
import { z } from 'zod';
import { DesktopClient } from './desktop-client.mjs';
import { renderScreenMirror } from './screen-mirror.mjs';
import { deployApp, resolveInside, uploadFile } from './deploy.mjs';
import { layeredRegistrar, MACHINE, parseLayers, SERVER_INSTRUCTIONS, TIME_CONTROL_NOTE } from './arcl-tools.mjs';

let layers;
try {
  layers = parseLayers();
} catch (error) {
  console.error(error.message);
  process.exit(2);
}

const client = new DesktopClient();
const server = new McpServer({ name: 'cyd-desktop', version: '0.0.1' }, { instructions: SERVER_INSTRUCTIONS });
// Registers a tool only when its ARCL layer is enabled (bridge/arcl-tools.mjs).
const tool = layeredRegistrar(server, layers);
const result = (value) => ({ content: [{ type: 'text', text: JSON.stringify(value) }] });
const invoke = (command, parameters = {}) => client.request(command, parameters).then(result);

async function paged(command, field, pageSize) {
  const items = [];
  let offset = 0;
  let first;
  for (let page = 0; page < 16; page += 1) {
    const response = await client.request(command, { offset, limit: pageSize });
    first ??= response;
    items.push(...(response[field] ?? []));
    if (response.next >= response.total || response.next <= offset) break;
    offset = response.next;
  }
  return { ...first, [field]: items, offset: 0, next: items.length };
}

const delay = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

// desktop_app_deploy reads app directories from the PC. It is confined to this
// root so the bridge cannot be used to read arbitrary local files.
const APPS_ROOT = process.env.CYD_DESKTOP_APPS_ROOT
  ?? fileURLToPath(new URL('../examples', import.meta.url));

tool('arcl_status', 'Report the machine id, enabled layers, current view and foreground app, and the MicroPython app runtime. time_control is "none": the board runs in real time and has no frame counter, so frame is null.', {}, async () => {
  const { version, ...status } = await client.request('desktop_status');
  const runtime = await client.request('desktop_runtime_status');
  return result({
    machine: MACHINE,
    running: true,
    state: 'running',
    frame: null,
    time_control: 'none',
    time_control_note: TIME_CONTROL_NOTE,
    layers,
    firmware: version,
    ...status,
    app_runtime: runtime,
  });
});
tool('cyd_ui_tree', 'Read the detailed currently visible UI tree with ids, roles, labels, bounds, and enabled state.', {},
  async () => result(await paged('desktop_ui_tree', 'nodes', 4)));
tool('cyd_apps_list', 'List the ids of the built-in views and native apps, as a fixed list kept in the firmware. Installed MicroPython apps are listed by cyd_installed_apps_list.', {}, () => invoke('desktop_apps_list'));
tool('cyd_installed_apps_list', 'List MicroPython apps installed on the microSD, including Home pin state.', {},
  async () => result(await paged('desktop_installed_apps_list', 'apps', 6)));
tool('cyd_launch', 'Open a built-in view (clock, calendar, scripts, settings, editor), a native app, or an installed MicroPython app by id. A MicroPython app is queued: success means it will start, not that it has.', { app_id: z.string().min(1) }, ({ app_id }) => invoke('desktop_launch', { app_id }));
tool('cyd_activate', 'Same as cyd_launch. Kept for clients written against the older name.', { app_id: z.string().min(1) }, ({ app_id }) => invoke('desktop_activate', { app_id }));
tool('cyd_home', 'Return the foreground application to Home.', {}, () => invoke('desktop_home'));
tool('cyd_run', 'Pause the cooperative MicroPython app and grant a bounded number of cyd.update() frames.', { frames: z.number().int().min(1).max(3600) }, ({ frames }) => invoke('desktop_run', { frames }));
tool('cyd_runtime_status', 'Read cooperative MicroPython runtime pause and frame state.', {}, () => invoke('desktop_runtime_status'));
tool('cyd_pause', 'Cooperatively pause the foreground MicroPython app at its next cyd.update().', {}, () => invoke('desktop_pause'));
tool('cyd_resume', 'Resume a cooperatively paused MicroPython app.', {}, () => invoke('desktop_resume'));
tool('cyd_step', 'Advance a cooperatively paused MicroPython app by one cyd.update() frame.', {}, () => invoke('desktop_step'));
tool('cyd_tap', 'Queue a synthetic tap at an absolute 320x240 display coordinate.', {
  x: z.number().int().min(0).max(319),
  y: z.number().int().min(0).max(239),
}, (parameters) => invoke('desktop_tap', parameters));
tool('cyd_input_state', 'Read physical touch state, last coordinate, and queued synthetic tap count.', {}, () => invoke('desktop_input_state'));
tool('cyd_input_macro', 'Queue a short sequence of coordinate taps with optional delays between steps.', {
  steps: z.array(z.object({
    x: z.number().int().min(0).max(319),
    y: z.number().int().min(0).max(239),
    delay_ms: z.number().int().min(0).max(5000).optional(),
  })).min(1).max(64),
}, async ({ steps }) => {
  const responses = [];
  for (const [index, step] of steps.entries()) {
    if (index && step.delay_ms) await delay(step.delay_ms);
    responses.push(await client.request('desktop_tap', { x: step.x, y: step.y }));
  }
  return result({ queued: responses.length, steps: responses });
});
tool('cyd_input_read', 'Read the most recent input event, such as tap.home or tap.<button id>, without clearing it.', {}, () => invoke('desktop_input_read'));
tool('cyd_input_clear', 'Read the most recent input event and clear it.', {}, () => invoke('desktop_input_clear'));
tool('cyd_diagnostics', 'Read desktop transport and health diagnostics. fingerprint: 1 starts summing the CRC-32 of everything sent to the LCD (spi_fingerprint, from zero), 0 stops it; two firmware builds that paint identically produce the same change over the same actions.', {
  fingerprint: z.number().int().min(0).max(1).optional(),
}, ({ fingerprint }) => invoke('desktop_diagnostics', fingerprint === undefined ? {} : { fingerprint }));
tool('cyd_logs', 'Read the retained recent UI, input, and application event log.', {},
  async () => result(await paged('desktop_logs', 'events', 4)));
tool('cyd_screen_mirror', 'Render a lightweight logical 320x240 PNG mirror from the retained UI tree. This is not an LCD pixel readback.', {}, async () => {
  const tree = await paged('desktop_ui_tree', 'nodes', 4);
  const image = renderScreenMirror(tree);
  return { content: [
    { type: 'image', data: image.toString('base64'), mimeType: 'image/png' },
    { type: 'text', text: JSON.stringify({ source: 'retained-ui-tree', physical_readback: false, view: tree.view, nodes: tree.nodes.length }) },
  ] };
});
const settingValues = {
  brightness: { dim: 0, balanced: 1, bright: 2 },
  theme: { blue: 0, violet: 1, teal: 2 },
  animation: { off: 0, fast: 1, smooth: 2 },
  keyboard_layout: { jis: 0, us: 1 },
};
tool('cyd_settings_get', 'Read brightness, color theme, animation speed, and Bluetooth keyboard layout settings.', {}, () => invoke('desktop_settings_get'));
tool('cyd_settings_set', 'Persist one or more settings. Omitted settings remain unchanged. keyboard_layout is the key layout of a Bluetooth keyboard.', {
  brightness: z.enum(['dim', 'balanced', 'bright']).optional(),
  theme: z.enum(['blue', 'violet', 'teal']).optional(),
  animation: z.enum(['off', 'fast', 'smooth']).optional(),
  keyboard_layout: z.enum(['jis', 'us']).optional(),
}, (parameters) => {
  if (!Object.keys(parameters).length) throw new Error('At least one setting is required');
  return invoke('desktop_settings_set', Object.fromEntries(
    Object.entries(parameters).map(([key, value]) => [key, settingValues[key][value]]),
  ));
});
tool('cyd_touch_calibrate_start', 'Show the first of three touch-calibration crosshairs. Tap each crosshair once to save the corrected mapping.', {}, () => invoke('desktop_touch_calibrate_start'));
tool('cyd_radio_status', 'Read the radio mode of this boot (off, wifi or bluetooth) and the mode stored for the next boot. Wi-Fi and Bluetooth are exclusive.', {}, () => invoke('desktop_radio_status'));
tool('cyd_radio_set', 'Store the radio mode (off, wifi or bluetooth) for the next boot. It applies after a restart; restart true restarts the device right after replying, which fails while a foreground app is running.', {
  mode: z.enum(['off', 'wifi', 'bluetooth']),
  restart: z.boolean().default(false),
}, ({ mode, restart }) => invoke('desktop_radio_set', { mode, restart: restart ? 1 : 0 }));
const KEY_NAME = /^(?:(?:CTRL|ALT|SHIFT)\+)*(?:[\x20-\x7e]|ENTER|BACKSPACE|DELETE|TAB|ESC|UP|DOWN|LEFT|RIGHT|HOME|END|PAGEUP|PAGEDOWN|INSERT|F(?:[1-9]|1[0-2]))$/;
tool('cyd_key_press', 'Press one key as a hardware keyboard would: the editor edits with it and a running app reads it with cyd.key(). Names: a character ("a", "A"), ENTER, BACKSPACE, DELETE, TAB, ESC, UP, DOWN, LEFT, RIGHT, HOME, END, PAGEUP, PAGEDOWN, INSERT, F1-F12, with CTRL+, ALT+ or SHIFT+ in front (CTRL+S). CTRL+C interrupts a running app.', {
  key: z.string().regex(KEY_NAME),
}, ({ key }) => invoke('desktop_key', { key }));
tool('cyd_type_text', 'Type text as hardware keyboard presses, one key per character; a newline is ENTER. Printable ASCII only.', {
  text: z.string().min(1).max(512).regex(/^[\x20-\x7e\n]*$/),
}, async ({ text }) => {
  for (const character of text) {
    await client.request('desktop_key', { key: character === '\n' ? 'ENTER' : character });
  }
  return result({ typed: text.length });
});
tool('cyd_bt_status', 'Read the Bluetooth keyboard state: connected and remembered keyboard, pairing progress with the PIN or passkey to type, and the results of the last scan. Bluetooth runs only in radio mode bluetooth.', {}, () => invoke('desktop_bt_status'));
tool('cyd_bt_scan', 'Search for Bluetooth Classic keyboards in pairing mode for the given seconds. Poll cyd_bt_status for results.', {
  seconds: z.number().int().min(1).max(30).default(10),
}, ({ seconds }) => invoke('desktop_bt_scan', { seconds }));
tool('cyd_bt_connect', 'Pair with a keyboard from the last scan by index. The screen shows a PIN or passkey (also in cyd_bt_status) that the user types on the keyboard, then Enter. Pairing a keyboard forgets the previous one.', {
  index: z.number().int().min(0).max(5),
}, ({ index }) => invoke('desktop_bt_connect', { index }));
tool('cyd_bt_forget', 'Disconnect and forget the remembered Bluetooth keyboard.', {}, () => invoke('desktop_bt_forget'));
tool('cyd_sd_status', 'Read whether the optional FAT microSD card was mounted safely at boot.', {}, () => invoke('desktop_sd_status'));
tool('cyd_package_upload', 'Atomically upload one file into /sd/apps/<app_id>/ after SHA-256 verification. Text by default; pass encoding "base64" for binary assets such as JPEG images. Existing files are kept unless replace is true. Fails while a foreground app is running.', {
  app_id: z.string().regex(/^[A-Za-z0-9_.-]{1,24}$/),
  file: z.string().regex(/^[A-Za-z0-9_.-]{1,24}$/),
  content: z.string().max(65536),
  encoding: z.enum(['utf8', 'base64']).default('utf8'),
  replace: z.boolean().default(false),
}, async ({ app_id, file, content, encoding, replace }) =>
  result(await uploadFile(client, app_id, { name: file, bytes: Buffer.from(content, encoding) }, { replace })));
tool('cyd_app_deploy', `Install or update a whole app directory - manifest.json, the entry file and every asset, binary included - after checking the manifest. app_dir is resolved inside CYD_DESKTOP_APPS_ROOT, which defaults to the repository's examples directory. Returns to Home first, because the device refuses uploads while an app runs.`, {
  app_dir: z.string().min(1).max(256),
  pin: z.boolean().default(false),
  launch: z.boolean().default(false),
  replace: z.boolean().default(true),
}, async ({ app_dir, pin, launch, replace }) =>
  result(await deployApp(client, await resolveInside(APPS_ROOT, app_dir), { pin, launch, replace })));
tool('cyd_app_error', 'Read the full traceback of the most recent MicroPython app failure. The event log keeps only a 48-character summary.', {}, () => invoke('desktop_app_error'));
tool('cyd_script_launch', 'Launch /sd/apps/<app_id>/main.py as the single foreground MicroPython app.', {
  app_id: z.string().regex(/^[A-Za-z0-9_.-]{1,24}$/),
}, ({ app_id }) => invoke('desktop_script_launch', { app_id }));
tool('cyd_app_delete', 'Permanently delete one installed MicroPython app and any Home shortcut that targets it.', {
  app_id: z.string().regex(/^[A-Za-z0-9_.-]{1,24}$/),
}, ({ app_id }) => invoke('desktop_app_delete', { app_id }));
tool('cyd_shortcuts_list', 'List the microSD-backed Home shortcuts currently loaded by the desktop.', {}, () => invoke('desktop_shortcuts_list'));
const shortcutSchema = {
  shortcut_id: z.string().regex(/^[A-Za-z0-9_.-]{1,24}$/),
  app_id: z.string().regex(/^[A-Za-z0-9_.-]{1,24}$/),
  title: z.string().regex(/^[A-Za-z0-9 _.-]{1,12}$/),
};
tool('cyd_shortcut_create', 'Create a new microSD-backed Home shortcut. Home shows two shortcuts on its first page and eight on each further page, 18 in all.', shortcutSchema,
  (parameters) => invoke('desktop_shortcut_create', parameters));
tool('cyd_shortcut_update', 'Update an existing microSD-backed Home shortcut.', shortcutSchema,
  (parameters) => invoke('desktop_shortcut_update', parameters));
tool('cyd_shortcut_delete', 'Delete a microSD-backed Home shortcut.', {
  shortcut_id: z.string().regex(/^[A-Za-z0-9_.-]{1,24}$/),
}, ({ shortcut_id }) => invoke('desktop_shortcut_delete', { shortcut_id }));
tool('cyd_time_set', 'Synchronize the CYD clock from a Unix epoch.', { epoch: z.number().int().min(946684800) }, ({ epoch }) => invoke('desktop_time_set', { epoch }));

await server.connect(new StdioServerTransport());
