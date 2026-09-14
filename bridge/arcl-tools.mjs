// How the MCP tools line up with the ARCL common specification (docs/external/arcl-common-spec_en.md, an external specification).
//
// CYD is a physical ESP32 board, not an emulator, so it offers only the part of
// the spec that fits: the common arcl_status, machine-specific cyd_* tools for
// everything else (spec 7.1, 7.6), layer-filtered tool lists (3.1) and tool
// descriptions that say which family, category and layer a tool belongs to (7.5).
// It has no frame-stepped timeline: see TIME_CONTROL_NOTE.

export const MACHINE = 'cyd';
export const LAYERS = ['L0', 'L1', 'L2', 'L3', 'L4'];

export const TIME_CONTROL_NOTE = 'CYD is a physical ESP32 board, not an emulator. The shell, native apps, video, '
  + 'touch and the clock run in real time and cannot be stopped; there is no machine frame counter and no save '
  + 'state. cyd_pause, cyd_step and cyd_run only hold the foreground MicroPython app at its next cyd.update().';

export const SERVER_INSTRUCTIONS = `ARCL server for machine "${MACHINE}" (CYD Desktop, ESP32-2432S028). `
  + 'Observe with cyd_ui_tree (semantic, cheap) or cyd_screen_mirror, act with cyd_tap, then observe again. '
  + 'Start with arcl_status, which reports the machine id, the enabled layers and the current view. '
  + TIME_CONTROL_NOTE;

// Every tool the bridge offers. Control tools are listed regardless of the
// selected layers (spec 3.3). A tool missing from this table cannot be registered.
export const TOOLS = {
  arcl_status: { category: 'Control', layer: null },

  cyd_runtime_status: { category: 'Control', layer: null },
  cyd_pause: { category: 'Control', layer: null },
  cyd_resume: { category: 'Control', layer: null },
  cyd_step: { category: 'Control', layer: null },
  cyd_run: { category: 'Control', layer: null },

  cyd_ui_tree: { category: 'Observation', layer: 'L0' },
  cyd_screen_mirror: { category: 'Observation', layer: 'L0' },
  cyd_input_state: { category: 'Observation', layer: 'L0' },
  cyd_input_read: { category: 'Observation', layer: 'L0' },
  cyd_tap: { category: 'Action', layer: 'L0' },
  cyd_input_macro: { category: 'Action', layer: 'L0' },
  cyd_input_clear: { category: 'Action', layer: 'L0' },
  cyd_key_press: { category: 'Action', layer: 'L0' },
  cyd_type_text: { category: 'Action', layer: 'L0' },

  cyd_apps_list: { category: 'Observation', layer: 'L1' },
  cyd_installed_apps_list: { category: 'Observation', layer: 'L1' },
  cyd_shortcuts_list: { category: 'Observation', layer: 'L1' },
  cyd_settings_get: { category: 'Observation', layer: 'L1' },
  cyd_launch: { category: 'Action', layer: 'L1' },
  cyd_activate: { category: 'Action', layer: 'L1' },
  cyd_home: { category: 'Action', layer: 'L1' },
  cyd_script_launch: { category: 'Action', layer: 'L1' },
  cyd_settings_set: { category: 'Action', layer: 'L1' },
  cyd_time_set: { category: 'Action', layer: 'L1' },
  cyd_package_upload: { category: 'Action', layer: 'L1' },
  cyd_app_deploy: { category: 'Action', layer: 'L1' },
  cyd_app_delete: { category: 'Action', layer: 'L1' },
  cyd_shortcut_create: { category: 'Action', layer: 'L1' },
  cyd_shortcut_update: { category: 'Action', layer: 'L1' },
  cyd_shortcut_delete: { category: 'Action', layer: 'L1' },

  cyd_diagnostics: { category: 'Observation', layer: 'L2' },
  cyd_logs: { category: 'Observation', layer: 'L2' },
  cyd_app_error: { category: 'Observation', layer: 'L2' },

  cyd_sd_status: { category: 'Observation', layer: 'L3' },
  cyd_radio_status: { category: 'Observation', layer: 'L3' },
  cyd_radio_set: { category: 'Action', layer: 'L3' },
  cyd_bt_status: { category: 'Observation', layer: 'L3' },
  cyd_bt_scan: { category: 'Action', layer: 'L3' },
  cyd_bt_connect: { category: 'Action', layer: 'L3' },
  cyd_bt_forget: { category: 'Action', layer: 'L3' },
  cyd_touch_calibrate_start: { category: 'Action', layer: 'L3' },
};

// Layers come from --mcp-layers=L0,L1 or CYD_MCP_LAYERS. Without either, every
// layer is enabled, so an existing client configuration keeps all its tools.
export function parseLayers(argv = process.argv.slice(2), env = process.env) {
  const flag = argv.find((argument) => argument.startsWith('--mcp-layers='));
  const raw = flag ? flag.slice('--mcp-layers='.length) : env.CYD_MCP_LAYERS;
  if (raw === undefined || raw.trim() === '') return [...LAYERS];
  const layers = raw.split(',').map((layer) => layer.trim().toUpperCase()).filter(Boolean);
  const unknown = layers.filter((layer) => !LAYERS.includes(layer));
  if (unknown.length) throw new Error(`unknown MCP layer(s): ${unknown.join(', ')} (expected ${LAYERS.join(', ')})`);
  return LAYERS.filter((layer) => layers.includes(layer));
}

export function describe(name, description) {
  const entry = TOOLS[name];
  const family = name.startsWith('arcl_') ? 'Common (arcl)' : `Machine-specific (${MACHINE})`;
  const where = entry.layer ?? 'any layer';
  return `${family}. ${entry.category}, ${where}. ${description}`;
}

// Returns a function with server.tool's arguments that registers only the
// tools whose layer is enabled.
export function layeredRegistrar(server, layers) {
  const registered = [];
  const register = (name, description, ...rest) => {
    const entry = TOOLS[name];
    if (!entry) throw new Error(`MCP tool ${name} has no ARCL classification in bridge/arcl-tools.mjs`);
    if (entry.layer !== null && !layers.includes(entry.layer)) return;
    registered.push(name);
    server.tool(name, describe(name, description), ...rest);
  };
  register.registered = registered;
  return register;
}
