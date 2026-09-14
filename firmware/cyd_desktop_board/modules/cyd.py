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
                          game_touch as _game_touch, runtime_keepalive,
                          runtime_ping as _runtime_ping,
                          video_begin as _video_begin, video_present as _video_present,
                          jpeg_present as _jpeg_present, jpeg_file as _jpeg_file)
from _manifest import API_LEVEL

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

_callbacks = {}
_app_root = None
_dirty = True
# Whether this app has drawn anything itself. Until it has, print() output is
# shown on screen (see _boot.py).
_ui_used = False
_video_used = False

def _mark_dirty():
    global _dirty, _ui_used
    _dirty = True
    _ui_used = True

def _set_app_root(app_id):
    global _app_root, _ui_used, _video_used
    _ui_used = False
    _video_used = False
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
    _game_begin(int(background_top) & 0xFFFF, int(background_bottom) & 0xFFFF,
                int(accent) & 0xFFFF)
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
