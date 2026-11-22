"""Small, cooperative UI API for one CYD Desktop MicroPython app."""

from _cyd_shell import (app_button, app_clear, app_event, app_log, app_primitive, app_stop_requested, app_text,
                         app_title, app_update, console_begin as _console_begin,
                         console_input as _console_input, console_keyboard as _console_keyboard,
                         console_key_at as _console_key_at, console_key_char as _console_key_char,
                         console_line as _console_line, console_options as _console_options,
                         game_begin as _game_begin, radio_mode,
                         key_read as _key_read, keyboard_connected as _keyboard_connected,
                         runtime_no_limit as _runtime_no_limit,
                         game_frame as _game_frame, game_sprite as _game_sprite,
                         game_triangle as _game_triangle, draw_triangle as _draw_triangle,
                         game_draw_stats as _game_draw_stats,
                         phys_reset as _phys_reset, phys_body as _phys_body,
                         phys_velocity as _phys_velocity, phys_step as _phys_step,
                         phys_state as _phys_state,
                         game_image_set as _game_image_set, game_bitmap as _game_bitmap,
                         overlay_clear as _overlay_clear, overlay_add as _overlay_add,
                         scene_begin as _scene_begin, scene_end as _scene_end, scene_image as _scene_image,
                         scene_overlay_clear as _scene_overlay_clear,
                         scene_overlay_add as _scene_overlay_add, scene_present as _scene_present,
                         buffer_copy as _buffer_copy,
                          game_touch as _game_touch, runtime_keepalive,
                          runtime_ping as _runtime_ping,
                          video_begin as _video_begin, video_present as _video_present,
                          jpeg_present as _jpeg_present, jpeg_file as _jpeg_file,
                          tile_begin as _tile_begin, tile_end as _tile_end, tile_palette as _tile_palette,
                          tile_frame as _tile_frame, tile_sprite as _tile_sprite, tile_text as _tile_text,
                          tile_rect as _tile_rect,
                          tile_present as _tile_present, keys_down as _keys_down)
from _manifest import API_LEVEL, safe_component as _safe_component

BLACK = 0x0000
NAVY = 0x000F
BLUE = 0x001F
GREEN = 0x07E0
CYAN = 0x07FF
RED = 0xF800
MAGENTA = 0xF81F
ORANGE = 0xFD20
YELLOW = 0xFFE0
WHITE = 0xFFFF

_PIXEL = 0
_LINE = 1
_RECT = 2
_FILL_RECT = 3
_CIRCLE = 4
_FILL_CIRCLE = 5

GAME_PLAYER = 0
GAME_ENEMY = 1
GAME_BULLET = 2
GAME_SHOT = 3
GAME_SPARK = 4

TILE_FLIP_X = 1
TILE_FLIP_Y = 2
TILE_BEHIND = 4

_callbacks = {}
_app_root = None
_dirty = True
# Whether this app has drawn anything itself. Until it has, print() output is
# shown on screen (see _boot.py).
_ui_used = False
_video_used = False
# The buffers a tile scene points into; kept alive while the scene is set.
_tile_buffers = None

def _mark_dirty():
    global _dirty, _ui_used
    _dirty = True
    _ui_used = True

def _tile_release():
    global _tile_buffers
    _tile_end()
    _tile_buffers = None
    _images_release()
    _scene_release()

def _set_app_root(app_id):
    global _app_root, _ui_used, _video_used
    _tile_release()
    _ui_used = False
    _video_used = False
    _compat_reset()
    _app_root = "/sd/apps/" + app_id + "/data"
    try:
        __import__("os").mkdir(_app_root)
    except OSError:
        pass

def _storage_key(key):
    key = str(key)
    if not key or len(key) > 24 or ".." in key:
        raise ValueError("invalid storage key")
    for character in key:
        if not (("a" <= character <= "z") or ("A" <= character <= "Z") or ("0" <= character <= "9") or character in "_-"):
            raise ValueError("invalid storage key")
    return key

def title(value):
    app_title(str(value))
    _mark_dirty()

def clear():
    _callbacks.clear()
    app_clear()
    _mark_dirty()

def text(x, y, value):
    result = app_text(int(x), int(y), str(value))
    if result:
        _mark_dirty()
    return result

def rgb(red, green, blue):
    """Pack 8-bit RGB channels into the display's RGB565 format."""
    red = max(0, min(255, int(red)))
    green = max(0, min(255, int(green)))
    blue = max(0, min(255, int(blue)))
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)

def _primitive(kind, x0, y0, x1, y1, color, stroke=1):
    result = app_primitive(kind, int(x0), int(y0), int(x1), int(y1),
                           int(color) & 0xFFFF, int(stroke))
    if result:
        _mark_dirty()
    return result

def pixel(x, y, color=WHITE):
    return _primitive(_PIXEL, x, y, 0, 0, color)

def line(x0, y0, x1, y1, color=WHITE, width=1):
    return _primitive(_LINE, x0, y0, x1, y1, color, width)

def rect(x, y, width, height, color=WHITE, stroke=1):
    return _primitive(_RECT, x, y, width, height, color, stroke)

def fill_rect(x, y, width, height, color=WHITE):
    return _primitive(_FILL_RECT, x, y, width, height, color)

def circle(x, y, radius, color=WHITE, stroke=1):
    return _primitive(_CIRCLE, x, y, radius, 0, color, stroke)

def fill_circle(x, y, radius, color=WHITE):
    return _primitive(_FILL_CIRCLE, x, y, radius, 0, color)

def game_begin(background_top=NAVY, background_bottom=BLACK, accent=CYAN):
    """Select the fixed-capacity, full game-area sprite renderer."""
    global _game_mode
    _game_begin(int(background_top) & 0xFFFF, int(background_bottom) & 0xFFFF,
                int(accent) & 0xFFFF)
    _game_mode = True
    _mark_dirty()

def game_frame(frame, score=0, lives=3, bombs=0):
    """Begin a game frame and clear its previous sprite batch."""
    _game_frame(max(0, int(frame)), max(0, int(score)), int(lives), int(bombs))
    _mark_dirty()

def game_sprite(kind, x, y, color=WHITE, size=3):
    """Append one of at most 128 native-rendered game sprites."""
    result = _game_sprite(int(kind), int(x), int(y), int(color) & 0xFFFF, int(size))
    if result:
        _mark_dirty()
    return result

def game_triangle(x0, y0, x1, y1, x2, y2, color=WHITE, wire=False):
    """Append one native rasterized triangle (flat-shaded solid or wireframe)."""
    result = _game_triangle(int(x0), int(y0), int(x1), int(y1), int(x2), int(y2),
                            int(color) & 0xFFFF, 1 if wire else 0)
    if result:
        _mark_dirty()
    return result

def game_quad(x0, y0, x1, y1, x2, y2, x3, y3, color=WHITE, wire=False):
    """Append one quad polygon as two native rasterized triangles."""
    r1 = _game_triangle(int(x0), int(y0), int(x1), int(y1), int(x2), int(y2),
                        int(color) & 0xFFFF, 1 if wire else 0)
    r2 = _game_triangle(int(x0), int(y0), int(x2), int(y2), int(x3), int(y3),
                        int(color) & 0xFFFF, 1 if wire else 0)
    if r1 or r2:
        _mark_dirty()
    return r1 and r2

def triangle(x0, y0, x1, y1, x2, y2, color=WHITE, fill=True):
    """Draw a 2D triangle (solid filled or wireframe outline)."""
    _draw_triangle(int(x0), int(y0), int(x1), int(y1), int(x2), int(y2),
                   int(color) & 0xFFFF, 0 if fill else 1)
    _mark_dirty()

def fill_triangle(x0, y0, x1, y1, x2, y2, color=WHITE):
    """Draw a 2D solid filled triangle."""
    triangle(x0, y0, x1, y1, x2, y2, color, fill=True)

def touch():
    """Return (pressed, x, y) for low-latency polling by a game loop."""
    return _game_touch()

def video_begin():
    """Enter the native MJP/JPEG player while retaining Python SD access."""
    global _dirty, _ui_used, _video_used
    result = _video_begin()
    if result:
        _dirty = False
        _ui_used = True
        _video_used = True
    return result

def video_present(jpeg):
    """Decode and synchronously present one baseline 240x240 JPEG frame."""
    result = _video_present(jpeg)
    if result == -4:
        return False
    if result != 0:
        raise OSError(result)
    return True

_TILE_ERRORS = {-1: "palette", -2: "tiles", -3: "sprites", -4: "map",
                -5: "no tile surface", -6: "display busy"}

def tile_begin(palette, tiles, sprites, tilemap, width, height, origin_y=0):
    """Show a full-screen scrolling tile map with sprites (API level 5).

    Cells are 16x16 at 4 bits per pixel (128 bytes each, high nibble first).
    palette: 16 big-endian RGB565 colours per palette, 1-4 palettes. tiles: map
    cells, colours from palette 0; tile 0 is empty. sprites: sprite cells, colour
    0 transparent. tilemap: width*height tile numbers, row by row, drawn from
    screen row origin_y. The buffers are used in place, so changing a byte of
    tilemap changes the map; they must not be resized.
    """
    global _tile_buffers, _ui_used, _dirty, _video_used
    result = _tile_begin(palette, tiles, sprites, tilemap, int(width), int(height), int(origin_y))
    if result != 0:
        raise ValueError("tile_begin: " + _TILE_ERRORS.get(result, str(result)))
    _tile_buffers = (tiles, sprites, tilemap)
    _ui_used = True
    _video_used = True
    _dirty = False
    return True

def tile_palette(palette):
    """Replace the palettes of the tile scene; the count must not change."""
    result = _tile_palette(palette)
    if result != 0:
        raise ValueError("tile_palette: " + _TILE_ERRORS.get(result, str(result)))

def tile_frame(scroll_x, scroll_y=0):
    """Start the next frame: scroll the map to world (x, y) and drop the sprites.

    scroll_y needs API level 6.
    """
    _tile_frame(scroll_x, scroll_y)

def tile_sprite(cell, x, y, flags=0):
    """Add a sprite cell at screen (x, y); at most 64 a frame.

    flags: TILE_FLIP_X, TILE_FLIP_Y, TILE_BEHIND, plus the palette number * 16.
    """
    return _tile_sprite(cell, x, y, flags)

def tile_text(slot, x, y, value, color=1, scale=1):
    """Set text slot 0-15 (0-7 before API level 6; 32 characters, palette 0
    colour); "" removes it. Kept across frames."""
    return _tile_text(int(slot), int(x), int(y), str(value), int(color), int(scale))

def tile_rect(slot, x, y, width, height, fill=0, border=-1):
    """Set rectangle slot 0-7 over the sprites and under the text (API level 6).

    fill and border are palette 0 colours; border -1 draws none, otherwise a
    2 pixel border with cut corners. Width or height 0 removes it. Kept across
    frames.
    """
    return _tile_rect(int(slot), int(x), int(y), int(width), int(height), int(fill), int(border))

def tile_present():
    """Show the frame. False once the tile view has left the screen."""
    result = _tile_present()
    if result == -4:
        return False
    if result != 0:
        raise OSError(result)
    return True

def keys_down():
    """The hardware keyboard keys held down now, as a tuple of names (API level 5).

    Characters without modifiers ("z", "1"), other keys as cyd.key() names
    ("LEFT", "ENTER"), then "SHIFT", "CTRL", "ALT". Most keyboards report up to
    six keys at once.
    """
    return _keys_down()

def jpeg(source, x=-1, y=-1):
    """Decode and display a JPEG from a file path (str) or raw bytes.
    If x or y is -1 (default), the image is centered on the screen.
    """
    if isinstance(source, str):
        result = _jpeg_file(str(source), int(x), int(y))
    else:
        result = _jpeg_present(source, int(x), int(y))
    if result != 0:
        raise OSError(result)
    return True

def console_begin():
    """Select the retained terminal + virtual keyboard renderer."""
    _console_begin()
    _mark_dirty()

def console_line(index, text, color=WHITE):
    """Set terminal line `index` (0..21); how many are visible depends on console_options()."""
    _console_line(int(index), str(text), int(color) & 0xFFFF)
    _mark_dirty()

def console_input(text, cursor_pos, prompt=None):
    """Set the input line, its cursor position and, optionally, the prompt (up to 7 characters)."""
    if prompt is None:
        _console_input(str(text), int(cursor_pos))
    else:
        _console_input(str(text), int(cursor_pos), str(prompt))
    _mark_dirty()

def console_options(keyboard=True, input=True):
    """Show or hide the on-screen keyboard and the input line.

    Lines visible: 10 with the keyboard, 21 with only the input line, 22 with neither.
    """
    _console_options(bool(keyboard), bool(input))
    _mark_dirty()

def console_keyboard(mode=0, pressed_key=-1):
    """Set keyboard mode (0:lower, 1:upper, 2:sym) and currently pressed key index."""
    _console_keyboard(int(mode), int(pressed_key))
    _mark_dirty()

# console_key_char() returns these for the keys that do not type text.
KEY_BACKSPACE = "\x08"
KEY_CAPS = "\x01"
KEY_SYMBOLS = "\x02"
KEY_ESCAPE = "\x1b"
KEY_ENTER = "\n"

def console_key_at(x, y):
    """Index (0..50) of the on-screen keyboard key at (x, y), or -1 outside it."""
    return _console_key_at(int(x), int(y))

def console_key_char(index, mode=0):
    """The character key `index` types in `mode`, or one of the KEY_* constants."""
    return _console_key_char(int(index), int(mode))

def button(identifier, x, y, width, height, label, on_tap=None):
    identifier = str(identifier)
    if on_tap is not None:
        _callbacks[identifier] = on_tap
    result = app_button(identifier, int(x), int(y), int(width), int(height), str(label))
    if result:
        _mark_dirty()
    return result

def log(value):
    app_log(str(value))
    _mark_dirty()

def storage_get(key, default=None):
    if _app_root is None:
        raise RuntimeError("app storage is unavailable")
    try:
        with open(_app_root + "/" + _storage_key(key), "r") as source:
            return source.read(512)
    except OSError:
        return default

def storage_put(key, value):
    if _app_root is None:
        raise RuntimeError("app storage is unavailable")
    value = str(value)
    if len(value) > 512:
        raise ValueError("storage value is limited to 512 bytes")
    with open(_app_root + "/" + _storage_key(key), "w") as output:
        output.write(value)

def checkpoint(value):
    """Atomically save a JSON-compatible app state for the next launch."""
    if _app_root is None:
        raise RuntimeError("app storage is unavailable")
    json = __import__("json")
    encoded = json.dumps(value)
    if len(encoded) > 4096:
        raise ValueError("checkpoint is limited to 4096 bytes")
    os = __import__("os")
    target = _app_root + "/checkpoint.json"
    temporary = target + ".part"
    previous = target + ".bak"
    for path in (temporary, previous):
        try:
            os.remove(path)
        except OSError:
            pass
    with open(temporary, "w") as output:
        output.write(encoded)
        try:
            output.flush()
        except Exception:
            pass
    # FAT cannot rename onto an existing name, so the old file has to move
    # aside first. Keeping it as .bak means there is always one complete
    # checkpoint on the card, whatever moment the power is cut.
    saved_previous = False
    try:
        os.rename(target, previous)
        saved_previous = True
    except OSError:
        pass
    os.rename(temporary, target)
    if saved_previous:
        try:
            os.remove(previous)
        except OSError:
            pass
    try:
        os.sync()
    except Exception:
        pass
    return True

def restore(default=None):
    """Restore the app-defined checkpoint, or return default when absent."""
    if _app_root is None:
        raise RuntimeError("app storage is unavailable")
    for name in ("/checkpoint.json", "/checkpoint.json.bak"):
        try:
            with open(_app_root + name, "r") as source:
                return __import__("json").loads(source.read(8192))
        except (OSError, ValueError):
            continue
    return default

def clear_checkpoint():
    if _app_root is None:
        raise RuntimeError("app storage is unavailable")
    try:
        __import__("os").remove(_app_root + "/checkpoint.json")
        return True
    except OSError:
        return False

def update():
    """Present batched draw calls and return False after a native Home escape."""
    global _dirty
    if _dirty:
        app_update()
        _dirty = False
    if _key_tracking:
        _poll_keys()
    if not runtime_keepalive():
        return False
    event = app_event()
    if event and event.startswith("tap."):
        callback = _callbacks.get(event[4:])
        if callback is not None:
            callback()
    return not app_stop_requested()

def should_stop():
    return app_stop_requested()

def run(step=None, interval_ms=30):
    """Cooperative foreground loop. A long press or Start/Home ends it."""
    import time
    while update():
        if step is not None:
            step()
        time.sleep_ms(max(10, int(interval_ms)))

def key():
    """The next key from a hardware keyboard, or None.

    A character is itself ("a", "A", "?"); other keys are named: "ENTER",
    "BACKSPACE", "DELETE", "TAB", "ESC", "UP", "DOWN", "LEFT", "RIGHT", "HOME",
    "END", "PAGEUP", "PAGEDOWN", "INSERT", "F1".."F12". Modifiers come first:
    "CTRL+S", "ALT+X", "SHIFT+TAB". Ctrl+C is not returned: it raises
    KeyboardInterrupt in the app. Keys arrive only while the app is in front.
    """
    return _key_read()

def keyboard_connected():
    """True while a Bluetooth keyboard is connected."""
    return _keyboard_connected()

def no_time_limit():
    """Let this app run longer than the 5 minute limit. The 15 s watchdog still applies."""
    _runtime_no_limit()

_wlan = None

def _require_wifi():
    # Wi-Fi and Bluetooth cannot share the heap, so the radio mode chosen in
    # Settings > Wireless decides at boot which one exists.
    mode = radio_mode()
    if mode != "wifi":
        raise RuntimeError("Wi-Fi is off: choose Wi-Fi in Settings > Wireless (radio mode is %s)" % mode)

def wifi_enabled():
    """Return True if the radio mode is Wi-Fi, so the wifi_* functions can work."""
    return radio_mode() == "wifi"

def wifi_init():
    global _wlan
    _require_wifi()
    if _wlan is None:
        import network
        _wlan = network.WLAN(network.STA_IF)
    return _wlan

def wifi_isconnected():
    """Return True if connected to a Wi-Fi access point with an assigned IP."""
    try:
        wlan = wifi_init()
        return bool(wlan.active() and wlan.isconnected())
    except Exception:
        return False

def wifi_ip():
    """Return current IPv4 address string, or None if not connected."""
    try:
        wlan = wifi_init()
        if wlan.active() and wlan.isconnected():
            return wlan.ifconfig()[0]
    except Exception:
        pass
    return None

def wifi_connect(ssid=None, password=None, timeout_ms=15000):
    """Connect to Wi-Fi station. If ssid is None, load from /sd/wifi.json."""
    import time
    if ssid is None:
        try:
            import json
            with open("/sd/wifi.json", "r") as f:
                cfg = json.load(f)
                ssid = cfg.get("ssid")
                password = cfg.get("password")
        except Exception as e:
            raise RuntimeError("Failed to load /sd/wifi.json: " + str(e))

    if not ssid:
        raise ValueError("SSID is required")

    wlan = wifi_init()
    if not wlan.active():
        wlan.active(True)

    if wlan.isconnected():
        return wlan.ifconfig()[0]

    wlan.connect(ssid, password if password else "")
    start = time.ticks_ms()
    while not wlan.isconnected():
        if time.ticks_diff(time.ticks_ms(), start) > timeout_ms:
            raise RuntimeError("Wi-Fi connection timeout")
        # Waiting on the radio is not a runaway loop, but it does outlast the
        # Task Watchdog, so keep it fed while we block.
        _runtime_ping()
        time.sleep_ms(100)

    return wlan.ifconfig()[0]

def wifi_disconnect():
    """Disconnect and deactivate the Wi-Fi interface."""
    try:
        wlan = wifi_init()
        if wlan.isconnected():
            wlan.disconnect()
        wlan.active(False)
    except Exception:
        pass

# Free heap on this board sits near 120 kB, and MicroPython's `bytes +=` copies
# the whole buffer each time, so an unbounded read is both a memory and a time
# problem. Refuse anything larger rather than dying halfway through.
HTTP_MAX_BYTES = 32768


def http_get(url, timeout=10):
    """Lightweight HTTP GET request returning string body.

    Blocks for up to `timeout` seconds in total, feeding the Task Watchdog
    while it waits, and refuses a body larger than HTTP_MAX_BYTES.
    """
    import socket
    import time
    _require_wifi()
    if url.startswith("http://"):
        url = url[7:]
    elif url.startswith("https://"):
        raise ValueError("Please use http:// for lightweight ESP32 requests")

    host_part, _, path_part = url.partition("/")
    path = "/" + path_part
    host, _, port_part = host_part.partition(":")
    port = int(port_part) if port_part else 80

    ai = socket.getaddrinfo(host, port, 0, socket.SOCK_STREAM)
    addr = ai[0][-1]

    s = socket.socket()
    # Per-recv timeout, bounded again below so a slow drip cannot outlast the
    # caller's overall budget.
    s.settimeout(min(timeout, 5))
    deadline = time.ticks_add(time.ticks_ms(), int(timeout * 1000))
    try:
        s.connect(addr)
        req = "GET " + path + " HTTP/1.0\r\nHost: " + host + "\r\nUser-Agent: CYD-Desktop/1.0\r\nConnection: close\r\n\r\n"
        s.send(req.encode())

        parts = []
        received = 0
        while True:
            _runtime_ping()
            if time.ticks_diff(deadline, time.ticks_ms()) <= 0:
                raise OSError("http_get timed out")
            chunk = s.recv(512)
            if not chunk:
                break
            received += len(chunk)
            if received > HTTP_MAX_BYTES:
                raise OSError("http_get response exceeds %d bytes" % HTTP_MAX_BYTES)
            parts.append(chunk)

        response = b"".join(parts)
        header_end = response.find(b"\r\n\r\n")
        if header_end != -1:
            head = response[:header_end]
            body = response[header_end + 4:]
        else:
            head = b""
            body = response
        status = 0
        if head.startswith(b"HTTP/"):
            try:
                status = int(head.split(b" ")[1])
            except (IndexError, ValueError):
                status = 0
        if status and not (200 <= status < 300):
            raise OSError("http_get failed with status %d" % status)
        return body.decode("utf-8")
    finally:
        s.close()


# ---- Helpers shared with the IS01 / 12HT port (API level 7) ----------------
# Same names, arguments and results as the Android port's cyd module, so apps
# move between them with fewer changes. The API levels of the two ports are
# numbered independently.

KEY_DOWN = 1
KEY_PRESSED = 2
KEY_RELEASED = 4

_last_touch_down = False
_key_tracking = False
_game_mode = False
_keys_previous = ()
_key_edges = {}
_last_error = None


def _compat_reset():
    """Forget the previous app's input edges and error (called per app start)."""
    global _last_touch_down, _key_tracking, _keys_previous, _key_edges, _last_error, _game_mode
    _last_touch_down = False
    _game_mode = False
    _key_tracking = False
    _keys_previous = ()
    _key_edges = {}
    _last_error = None
    # The physics world is native and outlives the app; start each app empty.
    _phys_reset(3, 0.0, -520.0, 0.0)


def _report(ok, reason):
    """Record why an init/load call failed, for last_error(). Returns ok."""
    global _last_error
    _last_error = None if ok else str(reason)
    return ok


def screen_size():
    """Return (width, height) of the screen in pixels: always (320, 240)."""
    return (320, 240)


def hypot(x, y):
    """Return the Euclidean norm sqrt(x*x + y*y)."""
    x = float(x)
    y = float(y)
    return (x * x + y * y) ** 0.5


def touch_pressed():
    """Return (x, y) while the screen is touched, or None."""
    pressed, x, y = _game_touch()
    return (x, y) if pressed else None


def touch_edge():
    """Return (x, y) once when a touch starts, or None."""
    global _last_touch_down
    pressed, x, y = _game_touch()
    if pressed and not _last_touch_down:
        _last_touch_down = True
        return (x, y)
    if not pressed:
        _last_touch_down = False
    return None


def touch_points():
    """Return the active touches as ((x, y, id), ...). The CYD panel reports one."""
    pressed, x, y = _game_touch()
    return ((x, y, 0),) if pressed else ()


def _poll_keys():
    global _keys_previous
    now = _keys_down()
    for name in now:
        if name not in _keys_previous:
            _key_edges[name] = _key_edges.get(name, 0) | KEY_PRESSED
    for name in _keys_previous:
        if name not in now:
            _key_edges[name] = _key_edges.get(name, 0) | KEY_RELEASED
    _keys_previous = now


def key_state(name, clear=True):
    """Return KEY_DOWN / KEY_PRESSED / KEY_RELEASED bits for one key.

    name is a cyd.keys_down() name ("w", " ", "LEFT", "SHIFT"). PRESSED and
    RELEASED are remembered until read; clear=False reads them without
    consuming them. Edges are tracked from the first call, at every call and
    every cyd.update().
    """
    global _key_tracking
    _key_tracking = True
    _poll_keys()
    name = str(name)
    bits = _key_edges.get(name, 0)
    if name in _keys_previous:
        bits |= KEY_DOWN
    if clear and name in _key_edges:
        del _key_edges[name]
    return bits


def game_sprites(records):
    """Queue packed <BhhHB sprite records (kind, x, y, color, size; 8 bytes each).

    Returns how many were accepted; the rest count as dropped in game_draw_stats().
    """
    import struct
    data = memoryview(records)
    if len(data) % 8:
        raise ValueError("records must be a multiple of 8 bytes")
    accepted = 0
    for offset in range(0, len(data), 8):
        kind, x, y, color, size = struct.unpack_from("<BhhHB", data, offset)
        if _game_sprite(kind, x, y, color, size):
            accepted += 1
    if accepted:
        _mark_dirty()
    return accepted


def game_draw_stats():
    """Return (sprites, triangles, dropped_sprites, dropped_triangles) for this frame."""
    return _game_draw_stats()


# (feature name, the function whose presence means the feature works here)
_FEATURE_FUNCS = (
    ("keyboard", "keyboard_connected"),
    ("image", "load_image"),
    ("overlay", "overlay_text"),
    ("phys", "phys_step"),
    ("sprite", "sprite_present"),
    ("tile", "tile_begin"),
    ("touch_points", "touch_points"),
    ("video", "video_begin"),
)


def features():
    """Return the sorted names of the optional features this firmware supports."""
    g = globals()
    names = [name for name, function in _FEATURE_FUNCS if function in g]
    if wifi_enabled():
        names.append("wifi")
    return tuple(sorted(names))


def last_error():
    """Return why the last init/load call failed, or None."""
    return _last_error


# ---- Physics (API level 8) --------------------------------------------------
# Spheres, axis-aligned boxes and infinite planes, without rotation. The native
# engine integrates and collides them; the app places bodies and draws the
# poses phys_state() reports. Same API as the IS01 / 12HT port.

PHYS_CAP = 48
PHYS_STRIDE = 40
PHYS_SPHERE = 1
PHYS_BOX = 2
PHYS_PLANE = 3


def phys_reset(dim=3, gravity=(0.0, -520.0, 0.0)):
    """Clear the physics world. dim is 2 or 3; gravity is in units per second squared."""
    _phys_reset(int(dim), float(gravity[0]), float(gravity[1]), float(gravity[2]))


def phys_body(body_id, shape, pos, size, mass=1.0, restitution=0.3, friction=0.4):
    """Place or replace one body and return its slot id (0..PHYS_CAP-1).

    sphere: size is the radius. box: size is (hx, hy, hz) half-extents.
    plane: pos is a point on the plane and size its normal (nx, ny, nz); bodies
    stay on the side the normal points to. mass <= 0 makes the body static.
    shape 0 removes the body. Raises ValueError for a bad slot or shape.
    """
    shape = int(shape)
    if shape == PHYS_SPHERE:
        hx, hy, hz = float(size), 0.0, 0.0
    elif shape == 0:
        hx = hy = hz = 0.0
    else:
        hx, hy, hz = float(size[0]), float(size[1]), float(size[2])
    return _phys_body(int(body_id), shape, float(pos[0]), float(pos[1]), float(pos[2]),
                      hx, hy, hz, float(mass), float(restitution), float(friction))


def phys_velocity(body_id, vel):
    """Set a dynamic body's velocity. False for a static or empty slot."""
    return bool(_phys_velocity(int(body_id), float(vel[0]), float(vel[1]), float(vel[2])))


def phys_step(dt):
    """Advance the simulation by dt seconds. Returns the number of contacts."""
    return _phys_step(float(dt))


def phys_state(buf):
    """Write PHYS_CAP records of PHYS_STRIDE bytes into buf; return the active body count.

    Each record is struct "<9fBBBB": x, y, z, vx, vy, vz, hx, hy, hz, shape,
    flags, id, active. flags bit 0 is set for a dynamic body.
    """
    return _phys_state(buf)


# ---- Images and HUD overlay (API level 9) ---------------------------------
# Same calls as the IS01 / 12HT port. Images are headerless little-endian
# RGB565 files in the app's own folder; colour 0 is transparent. Overlay items
# are drawn over the game frame, after the sprites and the score line, and are
# cleared by game_frame() and overlay_clear().

ALIGN_LEFT = 0
ALIGN_CENTER = 1
ALIGN_RIGHT = 2
IMAGE_CAP = 16
OVERLAY_CAP = 24

# Pixel buffers the native table points into, by slot; kept alive until release.
_images = [None] * IMAGE_CAP


def _images_release():
    for slot in range(IMAGE_CAP):
        if _images[slot] is not None:
            _game_image_set(slot, None, 0, 0)
            _images[slot] = None


def _app_file(name):
    name = str(name)
    if not _safe_component(name) or _app_root is None:
        raise ValueError("asset must be a file name in this app")
    return _app_root[:-len("/data")] + "/" + name


def load_image(name, width, height):
    """Load a headerless little-endian RGB565 image from this app's folder.

    Returns a bitmap id (0..IMAGE_CAP-1) for game_bitmap(). The pixels stay on
    the app's heap until the app ends. Raises ValueError for a bad size,
    OSError when the file is missing or short, MemoryError when the heap or the
    IMAGE_CAP slots run out; last_error() then says why.
    """
    width, height = int(width), int(height)
    try:
        if not 1 <= width <= 320 or not 1 <= height <= 240:
            raise ValueError("image size out of range")
        slot = _images.index(None) if None in _images else -1
        if slot < 0:
            raise MemoryError("all %d image slots are in use" % IMAGE_CAP)
        pixels = bytearray(width * height * 2)
        with open(_app_file(name), "rb") as source:
            count = source.readinto(pixels)
            extra = source.read(1)
        if count != len(pixels) or extra:
            raise OSError("%s is not %dx%d RGB565" % (name, width, height))
        _game_image_set(slot, pixels, width, height)
        _images[slot] = pixels
    except Exception as error:
        _report(False, error)
        raise
    _report(True, "")
    return slot


def game_bitmap(id, x, y, scale=100, flags=0):
    """Queue a loaded image centred on (x, y) at scale percent; flags bit 0 mirrors it.

    Shares the 128 sprite slots with game_sprite(). False when the id is not
    loaded or the frame is full.
    """
    result = _game_bitmap(int(id), int(x), int(y), int(scale), int(flags))
    if result:
        _mark_dirty()
    return result


def _overlay_to_scene():
    """Whether overlay calls go to the sprite scene rather than the game frame."""
    if _game_mode:
        return False
    _scene_enter()
    return True


def overlay_clear():
    """Remove the overlay text and rectangles queued for this frame."""
    if _overlay_to_scene():
        _scene_overlay_clear()
        return
    _overlay_clear()
    _mark_dirty()


def overlay_text(x, y, value, color=WHITE, scale=1, shadow=0x0821, align=ALIGN_LEFT):
    """Queue 5x7 text (up to 40 characters) over the game frame.

    scale is 1..6, shadow an RGB565 colour drawn one pixel per scale down and
    right (0 for none), align ALIGN_LEFT / ALIGN_CENTER / ALIGN_RIGHT about x.
    False when the frame already has OVERLAY_CAP overlay items.
    """
    add = _scene_overlay_add if _overlay_to_scene() else _overlay_add
    result = add(0, int(x), int(y), 0, 0, int(color) & 0xFFFF, str(value),
                 int(scale), int(shadow) & 0xFFFF, int(align))
    if result and add is _overlay_add:
        _mark_dirty()
    return result


def overlay_rect(x, y, width, height, color=WHITE, fill=True):
    """Queue a filled (or, with fill=False, outlined) rectangle over the game frame."""
    add = _scene_overlay_add if _overlay_to_scene() else _overlay_add
    result = add(1 if fill else 2, int(x), int(y), int(width), int(height),
                 int(color) & 0xFFFF, None, 1, 0, 0)
    if result and add is _overlay_add:
        _mark_dirty()
    return result


# ---- Sprite scene (API level 10) --------------------------------------------
# The IS01 / 12HT port's NSPR calls: full-screen frames of RGB565 sprites,
# rotated, scaled, mirrored and blended, back to front, with the overlay on
# top. Each sprite_present() is one frame, drawn 16 rows at a time; the first
# one switches to the full-screen scene view. Sprite pixels stay in the app's
# heap until the app ends.

SPRITE_CAP = 32
SPRITE_ITEMS = 128
SPRITE_RECORD = 22
BLEND_NORMAL = 0
BLEND_ADD = 1
BLEND_SUBTRACT = 2
BLEND_MULTIPLY = 3

_sprites = [None] * SPRITE_CAP
_scene_used = False
_SCENE_ERRORS = {-1: "not enough memory for the scene", -5: "no scene view", -6: "display busy"}


def _scene_release():
    global _scene_used
    _scene_end()
    for slot in range(SPRITE_CAP):
        _sprites[slot] = None
    _scene_used = False


def _scene_enter():
    global _scene_used, _ui_used, _video_used, _dirty, _game_mode
    if _scene_used:
        return
    result = _scene_begin()
    if result == -1:
        raise MemoryError(_SCENE_ERRORS[-1])
    if result != 0:
        raise OSError(_SCENE_ERRORS.get(result, str(result)))
    _scene_used = True
    _game_mode = False
    _ui_used = True
    _video_used = True  # a full-screen view: an error returns Home
    _dirty = False


def _sprite_store(pixels, width, height):
    if None not in _sprites:
        raise MemoryError("all %d sprite slots are in use" % SPRITE_CAP)
    slot = _sprites.index(None)
    if not _scene_image(slot, pixels, width, height):
        raise ValueError("sprite size out of range")
    _sprites[slot] = (pixels, width, height)
    return slot


def _sprite_size(width, height):
    width, height = int(width), int(height)
    if not 1 <= width <= 320 or not 1 <= height <= 240:
        raise ValueError("sprite size out of range")
    return width, height


def sprite_load(name, width, height):
    """Load a headerless little-endian RGB565 sprite from this app's folder.

    Returns its id (0..SPRITE_CAP-1) for sprite_present(). Colour 0 is
    transparent. Errors as for load_image(), recorded in last_error().
    """
    try:
        width, height = _sprite_size(width, height)
        pixels = bytearray(width * height * 2)
        with open(_app_file(name), "rb") as source:
            count = source.readinto(pixels)
            extra = source.read(1)
        if count != len(pixels) or extra:
            raise OSError("%s is not %dx%d RGB565" % (name, width, height))
        slot = _sprite_store(pixels, width, height)
    except Exception as error:
        _report(False, error)
        raise
    _report(True, "")
    return slot


def sprite_create(width, height, pixels=None):
    """Make a sprite from width*height*2 bytes of RGB565, or a transparent one.

    The pixels are copied; change them later with sprite_update().
    """
    width, height = _sprite_size(width, height)
    buffer = bytearray(width * height * 2)
    if pixels is not None:
        _buffer_copy(buffer, pixels)
    return _sprite_store(buffer, width, height)


def sprite_update(id, pixels):
    """Overwrite a sprite's pixels with the same number of bytes; shown from the next present."""
    entry = _sprites[int(id)] if 0 <= int(id) < SPRITE_CAP else None
    if entry is None:
        raise ValueError("invalid sprite id")
    _buffer_copy(entry[0], pixels)


def sprite_present(commands, clear=BLACK):
    """Draw a full-screen frame of up to SPRITE_ITEMS packed sprite commands.

    Each SPRITE_RECORD-byte record is struct "<HhhHHHhhHBBH": id, x, y,
    scale_x, scale_y (256 = 1x), angle (65536 = one clockwise turn), centre_x,
    centre_y (the image point placed at x, y and turned about), depth (smaller
    is nearer), alpha (0..255), blend (BLEND_*), flags (bit 0 mirrors left-
    right, bit 1 top-bottom). clear is the background colour. The overlay
    queued since the last present is drawn on top. Returns the record count,
    or -1 once the scene view has left the screen.
    """
    _scene_enter()
    result = _scene_present(commands, int(clear) & 0xFFFF)
    if result == -2:
        raise ValueError("invalid sprite command")
    if result == -4:
        return -1
    if result < 0:
        raise OSError(_SCENE_ERRORS.get(result, str(result)))
    return result
