# P13: expanded ARCL interface

English | [日本語](p13-arcl-interface_ja.md)

This phase implements the seven proposed control/observation levels without a
framebuffer or PSRAM dependency.

> The MCP tools were later renamed to follow the ARCL common specification: `desktop_status` became
> `arcl_status` and every other `desktop_*` tool became `cyd_*`. The firmware protocol commands keep
> their `desktop_*` names.

## Interface

1. `desktop_tap` queues an absolute display-coordinate tap. The bridge also
   provides `desktop_input_macro`, which sequences up to 64 taps with bounded
   delays, and `desktop_input_state` for physical/queued input observation.
2. `desktop_ui_tree` returns retained semantic nodes with stable id, role,
   label, bounds, and enabled state. Device responses are limited to four nodes
   per JSONL page; the MCP bridge joins all pages. A staging/published double
   buffer prevents clients from observing a partly rebuilt tree.
3. `desktop_installed_apps_list`, `desktop_script_launch`, shortcut operations,
   `desktop_app_delete`, and `desktop_settings_get/set` expose the native
   Scripts and Settings features. Settings remain the existing NVS-backed
   three-level brightness, theme, and animation values.
4. `desktop_screen_mirror` produces a 320x240 PNG on the PC from the retained
   semantic tree and the shared 5x7 glyph data. It is explicitly a logical
   mirror, not a readback of ILI9341 GRAM.
5. `desktop_logs` reads a 16-entry retained event ring. Expanded diagnostics
   include current/minimum heap, FreeRTOS task count, logical render count, SPI
   transaction/byte counters, synthetic tap count, watchdog state, and runtime
   frame state.
6. `desktop_pause`, `desktop_step`, `desktop_run(frames)`, and
   `desktop_resume` control cooperative MicroPython execution at `cyd.update()`
   boundaries. Paused wall time is excluded from the five-minute runtime limit;
   the native watchdog continues to be fed.
7. MicroPython applications can store a JSON-compatible state with
   `cyd.checkpoint(value)`, restore it on a later launch with
   `cyd.restore(default)`, and remove it with `cyd.clear_checkpoint()`. Payloads
   are bounded to 4096 bytes and use a temporary file before replacement.

The physical touch and synthetic tap routes meet at the same native hit tests.
The existing long-press Home escape and watchdog recovery remain authoritative.

## Resource and device verification

The final ESP32 application image is 768,912 bytes, leaving 1,262,704 bytes
(62 percent) free in the smallest app partition. ELF BSS is 20,393 bytes. At
the end of the full device test, free heap was 190,996 bytes and recorded
minimum free heap was 190,880 bytes. SHA-256 of the flashed application image
is `0e82db97dace2571ce2c719e4bc7e68b9a8b41dcae6d0002bd474eed96876d83`.

`npm test`, the standard COM9 integration, and `npm run integration:p13`
passed on 2026-09-10. The P13 run verified seven retained Home nodes, a
synthetic tap into Settings, an unchanged settings round trip, paged discovery
of seven installed applications, checkpoint values across two launches,
deletion of the disposable test package, a stable cooperative pause, one exact
step, two bounded frames, resume, and return to Home. The app-only flash kept
the existing NVS calibration/settings and microSD applications.
