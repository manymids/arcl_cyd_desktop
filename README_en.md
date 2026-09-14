# CYD Desktop Sandbox

English | [日本語](README_ja.md)

A small desktop environment for the ESP32-2432S028 CYD. Use the touchscreen,
USB serial or MCP to inspect state, launch apps, send input and transfer files.
A native ESP-IDF shell embeds MicroPython and runs applications from microSD.

## Features

- Home/Start, clock and calendar, Scripts, settings, text editor and on-screen keyboard
- Radio mode: off, Wi-Fi or Bluetooth (exclusive; switching restarts the board)
- Bluetooth keyboards: pairing page, JIS/US layouts, keys for the editor, console and apps, and the
  on-screen keyboard hidden while one is connected
- `print()` output and errors of scripts run from Scripts shown on screen; Ctrl+C interrupts
- Touch calibration stored in NVS and FAT/FAT32 microSD mounting
- MicroPython app installation, launch and deletion, with Home shortcuts
- USB JSON Lines observation and control, plus a stdio MCP bridge
- UI trees, diagnostics, logs, exception tracebacks and cooperative pause/step/resume
- 2D graphics, game sprites, JPEG display and MJP1 video playback
- Native App SDK, built-in `neon3d` raycast demo and `mjpplayer` video renderer

The LCD is 320×240. Normal UI rendering transfers tiles of up to 320×16 pixels
without requiring a full-screen framebuffer. Screen mirroring reconstructs a
PNG from the UI tree on the PC; it does not reproduce every pixel on the LCD.

The project is released under the MIT License; see "License" below.

## Developer documentation

- [Documentation index](docs/README_en.md): document purposes and historical-record caveats
- [cyd API reference](docs/cyd-api_en.md): MicroPython app development and installation
- [Native app guide](docs/native-app-guide_en.md): C++ implementation, registration and builds
- [Build from scratch](docs/build_en.md): WSL, installing ESP-IDF v5.5.2, fetching the included MicroPython v1.29.0 submodule, through to flashing
- [Build baseline](docs/p2-build-baseline_en.md): why ESP-IDF/MicroPython and WSL were chosen (a development-stage record)
- [MCP control and observation](docs/p13-arcl-interface_en.md): UI trees, input, diagnostics and execution control
- [MCP tools](docs/mcp-tools_en.md): every tool with its category, layer and arguments

## Target environment

| Item | Details |
| --- | --- |
| Board | ESP32-2432S028 CYD (ILI9341 version) only; see "Supported hardware" below |
| PC | Node.js and npm; host checks used Node.js 22.17.0 |
| Connection | USB serial, normally 115200 baud, 8N1 |
| Storage | FAT/FAT32 microSD |
| Keyboard (optional) | A Bluetooth Classic (HID) keyboard; BLE-only keyboards and mice are not supported |
| Firmware development | WSL Ubuntu 24.04, ESP-IDF and MicroPython |

## Supported hardware

Only the **ESP32-2432S028 ("CYD", Cheap Yellow Display) with the ILI9341 panel and the
resistive XPT2046 touch controller** is supported. Pins, panel initialisation and
orientation (320×240 landscape, BGR) are fixed in the firmware; nothing selects another board.

| Part | Specification and wiring |
| --- | --- |
| SoC | ESP32-D0WD-V3, 4 MiB flash, no PSRAM |
| Display | ILI9341 2.8" 320×240 on SPI2: SCK=14, MOSI=13, MISO=12, CS=15, DC=2, backlight=21 |
| Touch | XPT2046 (resistive, bit-banged): CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36 |
| microSD | SCK=18, MOSI=23, MISO=19, CS=5 (`machine.SDCard(slot=2)`) |
| USB serial | CH340 (VID `1a86` / PID `7523`) |

**Not supported:** versions with an ST7789 panel (such as the two-USB-port CYD2USB),
versions with capacitive touch, and variants with other screen sizes (such as the
ESP32-3248S035). On those the picture is garbled or colour-inverted, or touch does not work.

**First boot:** a board without a stored touch calibration opens the calibration screen
straight away. Tap the centre of the crosshair shown at three places in turn; the result is
saved and Home appears. To redo it, use Settings > SYSTEM > CALIBRATE (MCP
`cyd_touch_calibrate_start`). The calibration lives in NVS and survives flashing the app
partition.

MicroPython apps run one at a time in the foreground. "Sandbox" does not imply
security isolation: apps can use MicroPython file and hardware APIs. Run trusted apps.

## PC MCP bridge

Install dependencies from the repository root:

```powershell
npm ci
$env:CYD_DESKTOP_PORT = "COM6" # Replace with your device's port
npm start
```

Without an explicit port, the bridge detects CH340 devices with VID `1a86` and
PID `7523`. Specify a port when multiple CH340 devices are connected. Configure
your MCP client to launch `node bridge/index.mjs` with this repository as its
working directory. The transport is stdio; the bridge does not listen on a network port.

Tool names follow the naming rules of the [ARCL common specification](docs/external/arcl-common-spec_en.md)
(an external specification from a separate project; v0.5 is included):
the common `arcl_status`, and CYD-specific `cyd_*` tools. Main tools include `arcl_status`,
`cyd_launch`, `cyd_home`, `cyd_tap`, `cyd_input_macro`, `cyd_key_press`, `cyd_type_text`, `cyd_ui_tree`,
`cyd_settings_get/set`, `cyd_radio_status/set`, `cyd_bt_status/scan/connect/forget`, `cyd_logs`,
`cyd_diagnostics`, `cyd_app_error`, `cyd_pause/step/resume` and `cyd_screen_mirror`.

- `arcl_status` reports `machine: "cyd"`, the enabled layers and the current view.
- CYD is a physical board, not an emulator. The shell, native apps, video and the clock run in
  real time and cannot be stopped; there is no frame counter and no save state (`arcl_status`
  reports `frame: null` and `time_control: "none"`). `cyd_pause/step/run` only hold the
  foreground MicroPython app at `cyd.update()`.
- Limit the published tools by capability layer with `node bridge/index.mjs --mcp-layers=L0,L1`
  (or `CYD_MCP_LAYERS`). L0 is screen and input (including keys), L1 apps, settings and files,
  L2 logs and diagnostics, L3 the SD card, touch calibration, radio mode and Bluetooth. Control tools such as `arcl_status` and
  `cyd_pause` are always listed. Without a selection every layer is published.

Every tool, with its category, layer and arguments, is listed in [MCP tools](docs/mcp-tools_en.md).
The firmware's serial protocol commands keep their `desktop_*` names (see the JSON Lines examples below).

Do not open the same serial port simultaneously from the bridge, deployment
commands, firmware flashing tools or other programs.

## microSD and applications

Insert a FAT/FAT32 microSD before powering on. At boot, `machine.SDCard(slot=2)`
mounts it at `/sd` using SCK=18, MOSI=23, MISO=19 and CS=5. A mount failure does
not trigger formatting. Check `cyd_sd_status`; restart after changing cards.

Apps live in `/sd/apps/<app-id>/`. Create these two files in `apps/hello/` on the
PC to try a minimal app without image assets.

`manifest.json`:

```json
{
  "id": "hello",
  "title": "HELLO",
  "entry": "main.py",
  "version": "0.0.1"
}
```

`main.py`:

```python
import cyd
import time

cyd.clear()
cyd.title("HELLO")
cyd.text(16, 60, "Hello, CYD!")

while cyd.update():
    time.sleep_ms(20)
```

Stop the bridge to release the serial port, then deploy the directory:

```powershell
npm run deploy -- apps/hello --pin --launch
```

The manifest is validated on the PC, and both PC and device verify each file's
SHA-256. Existing files are replaced by default; use `--no-replace` to refuse
replacement. Deployment returns the device to Home first. Apps are flat
directories; recursive subdirectory deployment is unsupported.

For MCP `cyd_app_deploy`, set the parent directory containing your apps:

```powershell
$env:CYD_DESKTOP_APPS_ROOT = Join-Path (Get-Location) "apps"
npm start
```

With this configuration, `app_dir: "hello"` deploys `apps/hello`. The default
root is `examples/`. If samples are absent from the public release, configure
your own app directory as above. Use `cyd_package_upload` for individual
files; `encoding: "base64"` accepts binary data.

Call `cyd.update()` regularly to process input, rendering and stop requests.
Exit the loop when it returns `False`. An infinite loop that does not feed the
Watchdog is subject to a reset after 15 seconds. Stored string values are limited
to 512 bytes; JSON checkpoints to 4096 bytes. The Wi-Fi/HTTP helper APIs work only
in Wi-Fi radio mode (otherwise they raise `RuntimeError` with the reason), and
`cyd.http_get()` currently supports HTTP only, not HTTPS.

A script that draws nothing shows its `print()` output on screen, and a traceback
stays on screen when an exception stops it (Home closes it).

## Radio mode and Bluetooth keyboards

Wi-Fi and Bluetooth do not fit in memory together, so **Settings > WIRELESS**
chooses one. The default is OFF. Choosing another mode asks for confirmation and
takes effect after a restart. Free heap at Home is about 146 KB in off and Wi-Fi
mode and about 54 KB in Bluetooth mode.

| Mode | What it enables |
| --- | --- |
| OFF | Neither Wi-Fi nor Bluetooth; the most free memory |
| WI-FI | `cyd.wifi_*`, `cyd.http_get()`, `network` and `espnow` (credentials in `/sd/wifi.json`); in other modes the firmware refuses to start Wi-Fi |
| BLUETOOTH | A Bluetooth keyboard |

### Pairing

1. In Settings > WIRELESS choose **BLUETOOTH** and restart.
2. Open **KEYBOARD >** on the Wireless page, put the keyboard in pairing mode and tap **SCAN**.
3. Tap the keyboard in the list. Type the number shown in large digits (random each time) on
   the keyboard, then press Enter.
4. "PAIRED" means done. Choose the key layout with **KEYS JIS / KEYS US** (JIS by default).

One keyboard is remembered: pairing another forgets the previous one, and **FORGET**
removes it. A sleeping keyboard reconnects when a key is pressed, but keys pressed
before the link is back are lost. The symbol at the right end of the taskbar is green
while connected and amber while paired but disconnected, and a three-second notice
appears when the keyboard connects or goes.

### Using the keys

- **Editor**: while a keyboard is connected the on-screen keyboard is hidden and 16 lines are
  shown. Arrows, Home/End, PageUp/PageDown, Tab/Shift+Tab, Enter (keeps the indentation, one
  level deeper after `:`), Delete, Ctrl+S (save) and Ctrl+O (open). Files up to about 1 KB.
- **Console** (`examples/console`): 21 lines while connected; Up/Down recall history, a block
  started with `:` runs on an empty line, and Ctrl+C cancels the input or interrupts running code.
- **Apps**: read keys with `cyd.key()`. Ctrl+C raises `KeyboardInterrupt` in the running app.
  See "Hardware keyboard" in the [cyd API reference](docs/cyd-api_en.md).

### Limits in Bluetooth mode

- 24 KB stay reserved for Bluetooth, so the MicroPython heap does not grow into them. An app
  that needs more memory stops with `MemoryError`.
- MJP player video cannot play (it needs about 49 KB). Play it in off or Wi-Fi mode.
- Bluetooth Classic keyboards are supported. SKB-BT23BK has been used with this firmware;
  KB-PLT8990-K was tried, up to typing, with a test firmware during development.

## USB JSON Lines

UART0 is reserved for UTF-8 JSON Lines. The interactive REPL is disabled.
Send each JSON object on a separate line.

```json
{"id":"status-1","command":"desktop_status"}
{"id":"clock-1","command":"desktop_launch","app_id":"clock"}
{"id":"game-1","command":"desktop_launch","app_id":"neon3d"}
{"id":"home-1","command":"desktop_home"}
{"id":"sd-1","command":"desktop_sd_status"}
{"id":"script-1","command":"desktop_script_launch","app_id":"hello"}
{"id":"tree-1","command":"desktop_ui_tree","offset":0,"limit":4}
{"id":"radio-1","command":"desktop_radio_status"}
{"id":"key-1","command":"desktop_key","key":"CTRL+S"}
{"id":"pause-1","command":"desktop_pause"}
{"id":"step-1","command":"desktop_step"}
{"id":"resume-1","command":"desktop_resume"}
```

The `hello` launch example requires installing the application above first.

## Building and flashing firmware

To flash without building, use the prebuilt zip on the public repository's Releases
page. The steps, and how to make a release, are in
[Prebuilt firmware](docs/firmware-release_en.md).

The firmware is built with ESP-IDF v5.5.2 and MicroPython v1.29.0. The MicroPython
source is **included as a submodule at `firmware/micropython`**, upstream v1.29.0
unmodified (fetch it with `git submodule update --init firmware/micropython`).
ESP-IDF and the toolchain are not included and are installed separately.

Builds use WSL to avoid Windows command-line length limits during QSTR generation.
The commands below are for **incremental builds in an already configured
development environment**. Setting up WSL, installing ESP-IDF, fetching the
submodule and the first configuration are in
[Setting up the firmware build from scratch](docs/build_en.md).

From a Windows shell at the repository root (WSL starts in the Windows working directory):

```powershell
wsl -d Ubuntu-24.04 -e bash -c 'export IDF_TOOLS_PATH=~/cyd-desktop-vendor/.idf-tools && . ~/cyd-desktop-vendor/esp-idf/export.sh > /dev/null && cd firmware/micropython/ports/esp32 && idf.py -B build-cyd_desktop_board build'
```

The build configuration was created with these paths the first time (see the build guide):

| Setting | Project path |
| --- | --- |
| `MICROPY_BOARD_DIR` | `firmware/cyd_desktop_board` |
| `USER_C_MODULES` | `firmware/cyd_desktop_shell/micropython.cmake` |

The shell is split across `screen_*.cpp` and other files. The authoritative source
list is `firmware/cyd_desktop_shell/micropython.cmake`.

Copy the build output to the repository root as well:

```powershell
wsl -d Ubuntu-24.04 -e bash -c 'b=firmware/micropython/ports/esp32/build-cyd_desktop_board; cp "$b/micropython.bin" "$b/bootloader/bootloader.bin" "$b/partition_table/partition-table.bin" .'
```

The firmware includes Bluetooth, so its app partition is 3 MB
(`firmware/cyd_desktop_board/partitions-cyd.csv`). A blank board, or a board
flashed with firmware from before the Bluetooth build, needs the bootloader and
partition table as well. Verify the port and close other serial connections
first. NVS keeps its offset, so touch calibration and settings survive.

```powershell
python -m esptool --port COM6 --before default_reset write_flash 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 micropython.bin
```

After that, updating the app partition alone is enough:

```powershell
python -m esptool --port COM6 --before default_reset write_flash 0x10000 micropython.bin
```

## Development tests

Host-side checks need no device:

```bash
npm test
# C++ and Python checks; needs g++ and python3 in WSL (or npm run test:native)
wsl bash tools/run-native-tests.sh
```

On 2026-09-14, 59 JavaScript tests, seven C++ test groups and Python manifest
checks passed with no skips. `npm audit` reported no known vulnerabilities in
the npm dependencies at that time. These results do not guarantee the security
of the entire firmware.

The native test script treats compilation failures as `SKIP`. Check both
`failed` and `skipped`, not just the exit code. Without g++ (for example in Git
Bash on Windows) every test is reported as `SKIP`.

## License

Original project code: **MIT License — Copyright (c) 2026 manymids**.

- [LICENSE](LICENSE): MIT License for original project code
- [THIRD_PARTY_NOTICES_en.md](THIRD_PARTY_NOTICES_en.md): origins, conditions and distribution notes
- [licenses/](licenses/): original third-party license texts

The Adafruit-derived font is BSD 2-Clause; the MicroPython core is MIT.
Third-party components retain their own terms. Include the license materials
with firmware binaries and check notices required by additional components
actually included in the build. The current materials do not certify a completed
license audit of every linked component.
