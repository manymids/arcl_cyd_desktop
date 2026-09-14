# `cyd` API reference

English | [日本語](cyd-api_ja.md)

This reference describes the MicroPython application API for ARCL CYD Desktop.
It consolidates the descriptions previously spread across phase documents
(P9, P14, P19, P20 and others).

- Implementation: `firmware/cyd_desktop_board/modules/cyd.py`
- `bridge/sdk-docs.test.mjs` checks documented public functions and constants
  against the implementation. It also checks the capacity limits below.

## 1. Application layout

```text
/sd/apps/<app-id>/
    manifest.json   Optional application name and entry point
    main.py         Entry point; overridable with manifest.json entry
    *.jpg, etc.     Assets; application subdirectories cannot be deployed
    data/           Created automatically for cyd.storage_* and checkpoints
```

`<app-id>` is the directory name: 1–24 characters from `A-Z a-z 0-9 _ . -`.

### 1.1 manifest.json

```json
{
  "id": "console",
  "title": "CONSOLE",
  "entry": "main.py",
  "api": 4,
  "version": "0.0.1"
}
```

| Key | Required | Rule |
| --- | --- | --- |
| `title` | Yes | 1–12 characters from `A-Z a-z 0-9 space _ . -`; shown in Scripts and Home |
| `id` | | If present, must match the device application directory name |
| `entry` | | A `.py` filename; defaults to `main.py` |
| `api` | | Required API level, a positive integer; an app will not launch if this exceeds the firmware's `cyd.API_LEVEL`; defaults to 1 |
| `version` | | A 1–16 character display string; does not affect execution |

- Unknown keys, such as a misspelled `tittle`, are rejected.
- Apps without a manifest are valid. The app ID becomes the title and `main.py` is the entry point.
- An invalid manifest excludes the app from Scripts. `cyd_logs` records
  `skip <app-id>: <field>: <reason>`.
- Validation is implemented by `_manifest.py` on the device and
  `bridge/manifest.mjs` on the PC. Both use the shared cases in
  `firmware/cyd_desktop_board/tests/manifest-cases.json`.

### 1.2 Installation

Deploy the entire application directory. The PC validates the manifest before sending it.

```bash
node bridge/deploy.mjs examples/console --pin --launch
```

| Option | Meaning |
| --- | --- |
| `--pin` | Create a Home shortcut, or update the existing shortcut for the same app |
| `--launch` | Launch after deployment |
| `--no-replace` | Fail if a file with the same name exists; replacement is the default |
| `--port COM6` | Explicit serial port; otherwise use `CYD_DESKTOP_PORT`, then CH340 auto-detection |

If the local directory name differs from the intended app ID, set `id` in the
manifest. For example, `examples/pixel_barrage` installs as `pixelstorm`.
`README*`, `__pycache__/`, `*.pyc` and dotfiles are skipped.

To add one large asset:

```bash
node bridge/deploy.mjs --app mjpplayer --asset movie.mjp --fast
```

`--fast` temporarily switches the transfer to 921,600 bps.

MCP clients use `cyd_app_deploy` for directories and `cyd_package_upload`
for individual files; `encoding: "base64"` supports binary data.
`cyd_app_deploy` can only read within `CYD_DESKTOP_APPS_ROOT`, which defaults
to the repository's `examples/` directory.

## 2. Execution model

- Only one app runs in the foreground. Home, Start and a long press can request its termination.
- Drawing requests are batched and presented when **`cyd.update()` is called**.
- `cyd.update()` also feeds the Task Watchdog. Failing to feed it for 15 seconds
  resets the board and returns to Home. `cyd.wifi_connect()` and `cyd.http_get()`
  feed it while waiting, so callers do not need to call `update()` during those waits.
- Each launch has a five-minute runtime limit. Time spent paused through
  `cyd_pause` and related operations is excluded. An app meant to stay open,
  such as the console, lifts it with `cyd.no_time_limit()`.
- The app stops if free ESP-IDF heap falls below 8,192 bytes and has decreased
  by at least 4,096 bytes since that app started.
- The MicroPython heap grows into the ESP-IDF heap when it runs short and keeps
  that memory until reboot. In Bluetooth radio mode it stops growing 24 KB short
  of the end, which the keyboard needs to connect; an app that needs more stops
  with `MemoryError`. Free heap at Home is about 146 KB in off and Wi-Fi mode and
  about 54 KB in Bluetooth mode.
- Stop reasons are logged, including `Stopped: heap ...` and
  `Stopped: 5 minute runtime limit`.

### 2.1 Errors

- An unhandled exception produces a 48-character event-log summary:
  `ERROR: <type>: <message>`.
- The MCP tool `cyd_app_error` (protocol command `desktop_app_error`) retrieves the full recorded
  traceback of the most recent failure. Use the returned `app_id` to identify
  the app. Per-app records are saved at `/sd/desktop/errors/<app-id>.txt`
  (up to 1,023 characters).
- `sys.exit()` is treated as normal termination.
- After an unhandled exception the traceback stays on screen until Home is
  tapped (an app that was playing video returns Home, since the console cannot
  draw over the video surface).
- **Ctrl+C** on a hardware keyboard raises `KeyboardInterrupt` in the running
  app, even inside a busy loop such as `while True: pass`. Uncaught, the app
  stops with `Stopped: Ctrl+C`.

### 2.2 print()

While an app has drawn nothing, `print()` output fills the screen (22 lines),
so a script run from Scripts shows its results. Once the app starts drawing
(`cyd.title()` and the like), `print()` shows only its last line in the status
line, where `cyd.log()` writes.

### 2.3 Minimal application

```python
import cyd

count = 0

def draw():
    cyd.clear()
    cyd.title("COUNTER")
    cyd.text(16, 60, "COUNT %d" % count)
    cyd.button("add", 16, 120, 120, 40, "ADD", add)

def add():
    global count
    count += 1
    draw()          # Redraw only when state changes

draw()
cyd.run()           # Exit through Home, Start or a long press
```

`cyd.clear()` removes all text, buttons, shapes and button callbacks. Recreate
everything needed when redrawing. Clearing and rebuilding every frame transfers
the entire app area even if nothing changed; redraw only when state changes.

## 3. Display and windows

The display is 320×240. Apps draw below the title strip and above the taskbar,
which starts at y = 212. Colors are RGB565 integers; use `cyd.rgb()` to create them.

### 3.1 Constants

| Name | Value |
| --- | --- |
| `cyd.API_LEVEL` | API level supplied by the firmware; currently 4 |
| `cyd.BLACK` `cyd.NAVY` `cyd.BLUE` `cyd.GREEN` `cyd.CYAN` `cyd.RED` `cyd.MAGENTA` `cyd.ORANGE` `cyd.YELLOW` `cyd.WHITE` | RGB565 colors |
| `cyd.GAME_PLAYER` `cyd.GAME_ENEMY` `cyd.GAME_BULLET` `cyd.GAME_SHOT` `cyd.GAME_SPARK` | Sprite kinds, 0–4 |
| `cyd.HTTP_MAX_BYTES` | HTTP response size limit: 32,768 bytes |

### 3.2 Capacity limits

Calls exceeding these capacities return **`False` and are ignored**, rather than
raising an exception. Check their return values.

| Item | Limit | Firmware constant |
| --- | ---: | --- |
| Text entries | 10 | `kAppTextCapacity` |
| Buttons | 4 | `kAppButtonCapacity` |
| Shapes | 48 | `kAppPrimitiveCapacity` |
| Game sprites | 128 | `kGameSpriteCapacity` |
| Console lines | 22 | `kConsoleMaxLines` |

### 3.3 Basic UI

#### `cyd.title(value)`

Set the window title, up to 24 characters.

#### `cyd.clear()`

Remove all text, buttons, shapes and registered button callbacks.

#### `cyd.text(x, y, value)` → `bool`

Add one line of text, up to 48 characters. x is clamped to 16–300 and y to 47–199.

#### `cyd.button(identifier, x, y, width, height, label, on_tap=None)` → `bool`

Add a button. A tap calls `on_tap()` during the next `cyd.update()`.
`identifier` is limited to 16 characters and `label` to 24. x is clamped to
16–290, y to 47–190, width to 24–140 and height to 16–48.

#### `cyd.log(value)`

Append an event-log line. Text beyond 48 characters is truncated.

### 3.4 Shapes

Drawing calls return `True` on success. Coordinates are clamped to x −320–640
and y −240–480; parts outside the display are not drawn.

#### `cyd.rgb(red, green, blue)` → `int`

Convert three components in the range 0–255 to an RGB565 value.

#### `cyd.pixel(x, y, color=WHITE)` → `bool`
#### `cyd.line(x0, y0, x1, y1, color=WHITE, width=1)` → `bool`
#### `cyd.rect(x, y, width, height, color=WHITE, stroke=1)` → `bool`
#### `cyd.fill_rect(x, y, width, height, color=WHITE)` → `bool`
#### `cyd.circle(x, y, radius, color=WHITE, stroke=1)` → `bool`
#### `cyd.fill_circle(x, y, radius, color=WHITE)` → `bool`

## 4. Update loop

#### `cyd.update()` → `bool`

Present batched drawing, feed the Watchdog and process button taps.
Return **`False` when app termination is requested**; leave your loop then.

#### `cyd.should_stop()` → `bool`

Report whether termination was requested, without drawing or feeding the Watchdog.

#### `cyd.run(step=None, interval_ms=30)`

Repeatedly call `step()` and wait until `cyd.update()` returns `False`.
The minimum wait is 10 ms.

## 5. Game rendering

Native code renders the sprites without a full framebuffer.

#### `cyd.game_begin(background_top=NAVY, background_bottom=BLACK, accent=CYAN)`

Select game rendering with a two-color vertical background gradient.

#### `cyd.game_frame(frame, score=0, lives=3, bombs=0)`

Start a new frame and clear the previous sprite batch. `lives` is 0–5 and `bombs` is 0–3.

#### `cyd.game_sprite(kind, x, y, color=WHITE, size=3)` → `bool`

Add one sprite. `kind` is a `cyd.GAME_*` constant; `size` is 1–16.
Up to 128 sprites can be submitted per frame.

#### `cyd.touch()` → `(pressed, x, y)`

Return the current physical touch state for low-latency polling in a game loop.

## 6. Images and video

### 6.1 Result codes

Failures raise `OSError(<code>)`, except that `cyd.video_present()` returns `False` for −4.

| Code | Meaning |
| ---: | --- |
| -1 | Invalid argument, such as an empty path or fewer than four data bytes |
| -2 | Display busy; could not acquire it within one second |
| -3 | LCD output failed |
| -4 | No longer in the video view; means stop, for `cyd.video_present()` only |
| -5 | Could not open the file |
| -6 | Image remains too large for the display even at 1/8 scale |
| -7 | Not enough memory for the decoder buffers (about 14 KB); try again when memory is free |
| -11 and below | JPEG decoder error: TJpgDec JRESULT encoded as −(value)−10 |

#### `cyd.jpeg(source, x=-1, y=-1)` → `True`

Display a baseline JPEG. `source` is a path string or bytes. −1 for x and y
centers the image. Oversized images are automatically scaled to 1/2, 1/4 or 1/8.

#### `cyd.video_begin()` → `bool`

Switch to the native video view. Python continues reading the SD card and submitting frames.

#### `cyd.video_present(jpeg)` → `bool`

Display one 240×240 baseline JPEG frame. Return `False` after leaving the video view.
Creating fresh bytes each frame can cause allocations of up to 65 kB and garbage
collection. Allocate one buffer for the largest frame and reuse it with
`readinto()` and `memoryview`, as in `examples/mjp_player/main.py`.
The GC heap expands on the initial large allocation and retains that memory until reboot.
Video needs about 49 KB, so it cannot play in Bluetooth radio mode.

## 7. Console

Native code draws a text terminal with an on-screen keyboard. The app interprets key input.

#### `cyd.console_begin()`

Select console rendering, with the on-screen keyboard, the input line and the prompt `>>> `.

#### `cyd.console_options(keyboard=True, input=True)`

Show or hide the on-screen keyboard and the input line. Visible lines: 10 with the keyboard, 21 with only the
input line, 22 with neither (API level 4 and later).

#### `cyd.console_line(index, text, color=WHITE)`

Set terminal line `index` (0–21), up to 51 characters.

#### `cyd.console_input(text, cursor_pos, prompt=None)`

Set the input line (up to 51 characters) and cursor position (0–50). `prompt` also replaces the prompt, up to
7 characters (API level 4 and later).

#### `cyd.console_keyboard(mode=0, pressed_key=-1)`

Select lowercase (0), uppercase (1) or symbols (2), and the key index to highlight as pressed.

Key positions and characters come from the layout the native editor uses, so an app needs no key table of its
own (API level 3 and later).

#### `cyd.console_key_at(x, y)`

Return the index (0–50) of the key at screen coordinates `(x, y)`, or -1 outside the keyboard. Coordinates from
`cyd.touch()` can be passed directly.

#### `cyd.console_key_char(index, mode=0)`

Return the character key `index` types in `mode` (0 lowercase, 1 uppercase, 2 symbols). An out-of-range `index`
raises `ValueError`. Keys that do not type text return one of these constants:

| Constant | Key |
|---|---|
| `cyd.KEY_BACKSPACE` | BS |
| `cyd.KEY_CAPS` | CAP (toggle uppercase) |
| `cyd.KEY_SYMBOLS` | SYM (toggle symbols) |
| `cyd.KEY_ESCAPE` | ESC |
| `cyd.KEY_ENTER` | ENTER (`"\n"`) |

### 7.1 Hardware keyboard

Keys from a Bluetooth keyboard (paired in Settings > Wireless > KEYBOARD) go to whatever is in front: the editor
edits with them, and a running app reads them with `cyd.key()` (API level 4 and later). A held key repeats after
0.5 s. The key layout is chosen in Settings with KEYS JIS / KEYS US. While a keyboard is connected the editor
hides its on-screen keyboard and shows 16 lines, restoring it on disconnect. A console-style app checks
`cyd.keyboard_connected()` and switches with `cyd.console_options(keyboard=False)`, as `examples/console` does. The keyboard symbol at the
right end of the taskbar is green while connected and amber while paired but disconnected, and a three-second
notice appears when the keyboard connects or goes. A sleeping keyboard reconnects when a key is pressed, but keys
pressed before the link is back are lost.

#### `cyd.key()` → `str` or `None`

Return the next key, or `None`. A character key is the character itself (`"a"`, `"A"`, `"?"`); other keys are
named: `"ENTER"`, `"BACKSPACE"`, `"DELETE"`, `"TAB"`, `"ESC"`, `"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"`, `"HOME"`,
`"END"`, `"PAGEUP"`, `"PAGEDOWN"`, `"INSERT"`, `"F1"`–`"F12"`. Modifiers come first: `"CTRL+S"`, `"ALT+X"`,
`"SHIFT+TAB"`. Ctrl+C is not returned; it raises `KeyboardInterrupt`.

#### `cyd.keyboard_connected()` → `bool`

Return `True` while a Bluetooth keyboard is connected.

#### `cyd.no_time_limit()`

Lift this app's five-minute runtime limit. The 15-second watchdog still applies.

Editor keys: arrows, Home / End (with Ctrl: start / end of the text), PageUp / PageDown, Tab (to the next
multiple of 4), Shift+Tab (dedent), Enter (keeps the indentation, one level deeper after `:`), Delete,
Ctrl+S (save), Ctrl+O (open).

The protocol command `desktop_key {"key": "CTRL+S"}` (MCP `cyd_key_press`, `cyd_type_text`) sends keys by the
same names.

## 8. Storage

Data is stored in `/sd/apps/<app-id>/data/`.

#### `cyd.storage_put(key, value)`

Save a string. `key` is 1–24 characters from `A-Z a-z 0-9 _ -`;
`value` is limited to 512 bytes.

#### `cyd.storage_get(key, default=None)` → `str`

Return the saved string, or `default` if absent.

#### `cyd.checkpoint(value)` → `True`

Save a JSON-compatible value for the next launch, up to 4,096 bytes.
The save sequence is designed to retain the preceding checkpoint if power fails during writing.

#### `cyd.restore(default=None)`

Read the checkpoint, or return `default` if absent.

#### `cyd.clear_checkpoint()` → `bool`

Delete the checkpoint; return `True` when deleted.

## 9. Networking

Wi-Fi and Bluetooth do not fit in the heap together, so the radio mode in
Settings > Wireless (OFF / WI-FI / BLUETOOTH, initially OFF) decides at boot
which one exists. A change applies after a restart. When the radio mode is not
Wi-Fi, `cyd.wifi_init()`, `cyd.wifi_connect()` and `cyd.http_get()` raise
`RuntimeError` with the reason, `cyd.wifi_isconnected()` returns `False` and
`cyd.wifi_ip()` returns `None`. Using `network.WLAN()` or `espnow` directly does not
get around this: the firmware refuses to start Wi-Fi and raises
`OSError: Wi-Fi is off: ...`.

#### `cyd.radio_mode()` → `str`

Return this boot's radio mode: `"off"`, `"wifi"` or `"bluetooth"` (API level 4 and later).

#### `cyd.wifi_enabled()` → `bool`

Return `True` when the radio mode is Wi-Fi (API level 4 and later).

Wi-Fi credentials can be loaded from `/sd/wifi.json`:
`{"ssid": "...", "password": "..."}`.

#### `cyd.wifi_init()` → `WLAN`

Return the station-mode `network.WLAN`, creating it on first use.

#### `cyd.wifi_connect(ssid=None, password=None, timeout_ms=15000)` → `str`

Connect and return the IPv4 address. Omitting `ssid` loads `/sd/wifi.json`.
Raise `RuntimeError` on timeout. Feed the Watchdog while waiting.

#### `cyd.wifi_isconnected()` → `bool`

Return `True` when connected with an IP address.

#### `cyd.wifi_ip()` → `str` or `None`

Return the current IPv4 address.

#### `cyd.wifi_disconnect()`

Disconnect and deactivate the interface.

> Once started, the Wi-Fi driver retains approximately 49 kB after app exit;
> rebooting releases it.

#### `cyd.http_get(url, timeout=10)` → `str`

Perform an `http://` GET and return the body as a string. HTTPS is unsupported.
`timeout` specifies the overall time budget in seconds. Raise `OSError` when the
response exceeds `cyd.HTTP_MAX_BYTES` or the status is outside 2xx.
Feed the Watchdog while waiting.
