# MCP tools

English | [日本語](mcp-tools_ja.md)

The tools published by the MCP bridge (`node bridge/index.mjs`).
**This document is generated from the bridge by `npm run docs:tools`. Do not edit it by hand.**
`bridge/mcp-tools-doc.test.mjs` fails when it no longer matches the bridge.

- **Names** follow the [ARCL common specification](external/arcl-common-spec_en.md) (an external specification): `arcl_*` for the common API, `cyd_*` for CYD-specific tools.
- **Category**: Observation only reads, Action changes state, Control controls execution.
- **Layers**: choose the published layers with `node bridge/index.mjs --mcp-layers=L0,L1` (or `CYD_MCP_LAYERS`). Without a selection every layer is published. Control tools are always published.
- **Time**: CYD is a physical board, not an emulator. The shell, native apps, video and the clock run in real time. `cyd_pause` and its relatives only hold the foreground MicroPython app at `cyd.update()`.
- **Protocol**: the firmware's serial protocol commands are named `desktop_*`. They are separate from the MCP tool names.

42 tools in all.

## Control (always published)

| Tool | Category | Arguments | Description |
| --- | --- | --- | --- |
| `arcl_status` | Control | none | Report the machine id, enabled layers, current view and foreground app, and the MicroPython app runtime. time_control is "none": the board runs in real time and has no frame counter, so frame is null. |
| `cyd_run` | Control | `frames`: integer, 1–3600 | Pause the cooperative MicroPython app and grant a bounded number of cyd.update() frames. |
| `cyd_runtime_status` | Control | none | Read cooperative MicroPython runtime pause and frame state. |
| `cyd_pause` | Control | none | Cooperatively pause the foreground MicroPython app at its next cyd.update(). |
| `cyd_resume` | Control | none | Resume a cooperatively paused MicroPython app. |
| `cyd_step` | Control | none | Advance a cooperatively paused MicroPython app by one cyd.update() frame. |

## L0 Screen and input

| Tool | Category | Arguments | Description |
| --- | --- | --- | --- |
| `cyd_ui_tree` | Observation | none | Read the detailed currently visible UI tree with ids, roles, labels, bounds, and enabled state. |
| `cyd_tap` | Action | `x`: integer, 0–319<br>`y`: integer, 0–239 | Queue a synthetic tap at an absolute 320x240 display coordinate. |
| `cyd_input_state` | Observation | none | Read physical touch state, last coordinate, and queued synthetic tap count. |
| `cyd_input_macro` | Action | `steps`: array, 1–64 items of {x, y, delay_ms?} | Queue a short sequence of coordinate taps with optional delays between steps. |
| `cyd_input_read` | Observation | none | Read the most recent input event, such as tap.home or tap.&lt;button id&gt;, without clearing it. |
| `cyd_input_clear` | Action | none | Read the most recent input event and clear it. |
| `cyd_screen_mirror` | Observation | none | Render a lightweight logical 320x240 PNG mirror from the retained UI tree. This is not an LCD pixel readback. |
| `cyd_key_press` | Action | `key`: string, `^(?:(?:CTRL|ALT|SHIFT)\+)*(?:[\x20-\x7e]|ENTER|BACKSPACE|DELETE|TAB|ESC|UP|DOWN|LEFT|RIGHT|HOME|END|PAGEUP|PAGEDOWN|INSERT|F(?:[1-9]|1[0-2]))$` | Press one key as a hardware keyboard would: the editor edits with it and a running app reads it with cyd.key(). Names: a character ("a", "A"), ENTER, BACKSPACE, DELETE, TAB, ESC, UP, DOWN, LEFT, RIGHT, HOME, END, PAGEUP, PAGEDOWN, INSERT, F1-F12, with CTRL+, ALT+ or SHIFT+ in front (CTRL+S). CTRL+C interrupts a running app. |
| `cyd_type_text` | Action | `text`: string, 1–512 chars, `^[\x20-\x7e\n]*$` | Type text as hardware keyboard presses, one key per character; a newline is ENTER. Printable ASCII only. |

## L1 Apps, settings and files

| Tool | Category | Arguments | Description |
| --- | --- | --- | --- |
| `cyd_apps_list` | Observation | none | List the ids of the built-in views and native apps, as a fixed list kept in the firmware. Installed MicroPython apps are listed by cyd_installed_apps_list. |
| `cyd_installed_apps_list` | Observation | none | List MicroPython apps installed on the microSD, including Home pin state. |
| `cyd_launch` | Action | `app_id`: string | Open a built-in view (clock, calendar, scripts, settings, editor), a native app, or an installed MicroPython app by id. A MicroPython app is queued: success means it will start, not that it has. |
| `cyd_activate` | Action | `app_id`: string | Same as cyd_launch. Kept for clients written against the older name. |
| `cyd_home` | Action | none | Return the foreground application to Home. |
| `cyd_settings_get` | Observation | none | Read brightness, color theme, animation speed, and Bluetooth keyboard layout settings. |
| `cyd_settings_set` | Action | `brightness`: string, `"dim"` / `"balanced"` / `"bright"`, optional<br>`theme`: string, `"blue"` / `"violet"` / `"teal"`, optional<br>`animation`: string, `"off"` / `"fast"` / `"smooth"`, optional<br>`keyboard_layout`: string, `"jis"` / `"us"`, optional | Persist one or more settings. Omitted settings remain unchanged. keyboard_layout is the key layout of a Bluetooth keyboard. |
| `cyd_package_upload` | Action | `app_id`: string, `^[A-Za-z0-9_.-]{1,24}$`<br>`file`: string, `^[A-Za-z0-9_.-]{1,24}$`<br>`content`: string, up to 65536 chars<br>`encoding`: string, `"utf8"` / `"base64"`, default `"utf8"`<br>`replace`: boolean, default `false` | Atomically upload one file into /sd/apps/&lt;app_id&gt;/ after SHA-256 verification. Text by default; pass encoding "base64" for binary assets such as JPEG images. Existing files are kept unless replace is true. Fails while a foreground app is running. |
| `cyd_app_deploy` | Action | `app_dir`: string, 1–256 chars<br>`pin`: boolean, default `false`<br>`launch`: boolean, default `false`<br>`replace`: boolean, default `true` | Install or update a whole app directory - manifest.json, the entry file and every asset, binary included - after checking the manifest. app_dir is resolved inside CYD_DESKTOP_APPS_ROOT, which defaults to the repository's examples directory. Returns to Home first, because the device refuses uploads while an app runs. |
| `cyd_script_launch` | Action | `app_id`: string, `^[A-Za-z0-9_.-]{1,24}$` | Launch /sd/apps/&lt;app_id&gt;/main.py as the single foreground MicroPython app. |
| `cyd_app_delete` | Action | `app_id`: string, `^[A-Za-z0-9_.-]{1,24}$` | Permanently delete one installed MicroPython app and any Home shortcut that targets it. |
| `cyd_shortcuts_list` | Observation | none | List the microSD-backed Home shortcuts currently loaded by the desktop. |
| `cyd_shortcut_create` | Action | `shortcut_id`: string, `^[A-Za-z0-9_.-]{1,24}$`<br>`app_id`: string, `^[A-Za-z0-9_.-]{1,24}$`<br>`title`: string, `^[A-Za-z0-9 _.-]{1,12}$` | Create a new microSD-backed Home shortcut. Home shows two shortcuts on its first page and eight on each further page, 18 in all. |
| `cyd_shortcut_update` | Action | `shortcut_id`: string, `^[A-Za-z0-9_.-]{1,24}$`<br>`app_id`: string, `^[A-Za-z0-9_.-]{1,24}$`<br>`title`: string, `^[A-Za-z0-9 _.-]{1,12}$` | Update an existing microSD-backed Home shortcut. |
| `cyd_shortcut_delete` | Action | `shortcut_id`: string, `^[A-Za-z0-9_.-]{1,24}$` | Delete a microSD-backed Home shortcut. |
| `cyd_time_set` | Action | `epoch`: integer, ≥ 946684800 | Synchronize the CYD clock from a Unix epoch. |

## L2 Logs and diagnostics

| Tool | Category | Arguments | Description |
| --- | --- | --- | --- |
| `cyd_diagnostics` | Observation | `fingerprint`: integer, 0–1, optional | Read desktop transport and health diagnostics. fingerprint: 1 starts summing the CRC-32 of everything sent to the LCD (spi_fingerprint, from zero), 0 stops it; two firmware builds that paint identically produce the same change over the same actions. |
| `cyd_logs` | Observation | none | Read the retained recent UI, input, and application event log. |
| `cyd_app_error` | Observation | none | Read the full traceback of the most recent MicroPython app failure. The event log keeps only a 48-character summary. |

## L3 Hardware

| Tool | Category | Arguments | Description |
| --- | --- | --- | --- |
| `cyd_touch_calibrate_start` | Action | none | Show the first of three touch-calibration crosshairs. Tap each crosshair once to save the corrected mapping. |
| `cyd_radio_status` | Observation | none | Read the radio mode of this boot (off, wifi or bluetooth) and the mode stored for the next boot. Wi-Fi and Bluetooth are exclusive. |
| `cyd_radio_set` | Action | `mode`: string, `"off"` / `"wifi"` / `"bluetooth"`<br>`restart`: boolean, default `false` | Store the radio mode (off, wifi or bluetooth) for the next boot. It applies after a restart; restart true restarts the device right after replying, which fails while a foreground app is running. |
| `cyd_bt_status` | Observation | none | Read the Bluetooth keyboard state: connected and remembered keyboard, pairing progress with the PIN or passkey to type, and the results of the last scan. Bluetooth runs only in radio mode bluetooth. |
| `cyd_bt_scan` | Action | `seconds`: integer, 1–30, default `10` | Search for Bluetooth Classic keyboards in pairing mode for the given seconds. Poll cyd_bt_status for results. |
| `cyd_bt_connect` | Action | `index`: integer, 0–5 | Pair with a keyboard from the last scan by index. The screen shows a PIN or passkey (also in cyd_bt_status) that the user types on the keyboard, then Enter. Pairing a keyboard forgets the previous one. |
| `cyd_bt_forget` | Action | none | Disconnect and forget the remembered Bluetooth keyboard. |
| `cyd_sd_status` | Observation | none | Read whether the optional FAT microSD card was mounted safely at boot. |
