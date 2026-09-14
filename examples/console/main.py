"""MicroPython Interactive Console for CYD Desktop.

Type with the on-screen keyboard or a Bluetooth keyboard. With a keyboard:
Up/Down recall history, Left/Right/Home/End move the cursor, Tab indents,
Ctrl+C cancels the input or interrupts running code. A line ending in ':'
(or with open brackets) starts a block; an empty line runs it.
"""

import os
import sys
import time
import cyd

C_WHITE = cyd.WHITE
C_GREEN = cyd.GREEN
C_CYAN = cyd.CYAN
C_RED = cyd.RED
C_YELLOW = cyd.YELLOW

# Lines shown above the input: 10 with the on-screen keyboard, 21 without it.
LINES_WITH_KEYBOARD = 10
LINES_WITHOUT_KEYBOARD = 21
MAX_LINES = LINES_WITH_KEYBOARD
COLUMNS = 52
HISTORY_SIZE = 20
PROMPT = ">>> "
CONTINUATION = "... "

lines = []
input_buffer = ""
cursor = 0
block = []          # lines of a multi-line statement being typed
history = []
history_index = 0   # len(history) means "the line being typed"
kb_mode = 0
last_executed_code = ""


def log(text, color=C_WHITE):
    global lines
    for raw_line in str(text).split("\n"):
        while len(raw_line) > COLUMNS:
            lines.append((raw_line[:COLUMNS], color))
            raw_line = raw_line[COLUMNS:]
        lines.append((raw_line, color))
    # Keep enough to fill the taller view when the on-screen keyboard goes away.
    if len(lines) > LINES_WITHOUT_KEYBOARD:
        lines = lines[-LINES_WITHOUT_KEYBOARD:]
    refresh_lines()


def refresh_lines():
    shown = lines[-MAX_LINES:]
    for i in range(MAX_LINES):
        if i < len(shown):
            cyd.console_line(i, shown[i][0], shown[i][1])
        else:
            cyd.console_line(i, "", C_WHITE)


def use_hardware_keyboard(connected):
    """Hide the on-screen keyboard while a Bluetooth keyboard is connected."""
    global MAX_LINES, touch_down_key
    MAX_LINES = LINES_WITHOUT_KEYBOARD if connected else LINES_WITH_KEYBOARD
    touch_down_key = -1
    cyd.console_options(keyboard=not connected)
    cyd.console_keyboard(kb_mode, -1)
    refresh_lines()
    refresh_input()


def my_print(*args, sep=" ", end="\n", file=None):
    text = sep.join(str(a) for a in args)
    if file is not None:
        file.write(text + end)
        return
    log(text, C_WHITE)


user_globals = {
    "__name__": "__main__",
    "cyd": cyd,
    "os": os,
    "sys": sys,
    "time": time,
    "print": my_print,
}


def refresh_input():
    prompt = CONTINUATION if block else PROMPT
    width = COLUMNS - len(prompt) - 1
    # Scroll the visible part so the cursor stays on screen.
    start = max(0, cursor - width)
    cyd.console_input(input_buffer[start:start + width], cursor - start, prompt)


def set_input(text, position=None):
    global input_buffer, cursor
    input_buffer = text
    cursor = len(text) if position is None else max(0, min(len(text), position))
    refresh_input()


def show_error(err):
    log("%s: %s" % (type(err).__name__, err), C_RED)


def needs_more(source):
    """True while a statement is visibly unfinished: open brackets or a trailing ':'."""
    depth = 0
    quote = None
    for ch in source:
        if quote:
            if ch == quote:
                quote = None
        elif ch in "'\"":
            quote = ch
        elif ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
    stripped = source.rstrip()
    return depth > 0 or stripped.endswith(":") or stripped.endswith("\\")


def run_python(source):
    try:
        try:
            code = compile(source, "<stdin>", "eval")
        except SyntaxError:
            code = None
        if code is not None:
            result = eval(code, user_globals)
            if result is not None:
                log(repr(result), C_CYAN)
        else:
            exec(compile(source, "<stdin>", "exec"), user_globals)
    except KeyboardInterrupt:
        log("KeyboardInterrupt", C_RED)
    except Exception as err:
        show_error(err)


def execute_command(cmd):
    global last_executed_code
    if cmd == "/help":
        log("Commands: /ls, /cat, /load, /save, /clear", C_YELLOW)
        log("Or enter any Python code e.g. 1+1, a=10", C_YELLOW)
    elif cmd == "/clear":
        lines.clear()
        refresh_lines()
    elif cmd.startswith("/ls"):
        parts = cmd.split(None, 1)
        path = parts[1] if len(parts) > 1 else "/sd"
        try:
            entries = os.listdir(path)
            log("Dir " + path + ": " + ", ".join(entries[:10]), C_CYAN)
        except Exception as err:
            log("Error: " + str(err), C_RED)
    elif cmd.startswith("/cat "):
        filename = cmd[5:].strip()
        try:
            with open(filename, "r") as f:
                log(f.read(200), C_CYAN)
        except Exception as err:
            log("Error: " + str(err), C_RED)
    elif cmd.startswith("/load "):
        filename = cmd[6:].strip()
        try:
            with open(filename, "r") as f:
                code_text = f.read()
            exec(code_text, user_globals)
            log("Loaded & ran: " + filename, C_CYAN)
        except KeyboardInterrupt:
            log("KeyboardInterrupt", C_RED)
        except Exception as err:
            log("Load Error: " + str(err), C_RED)
    elif cmd.startswith("/save "):
        filename = cmd[6:].strip()
        try:
            with open(filename, "w") as f:
                f.write(last_executed_code)
            log("Saved to " + filename, C_CYAN)
        except Exception as err:
            log("Save Error: " + str(err), C_RED)
    else:
        last_executed_code = cmd
        run_python(cmd)


def remember(text):
    global history_index
    if text and (not history or history[-1] != text):
        history.append(text)
        del history[:-HISTORY_SIZE]
    history_index = len(history)


def submit():
    """Enter: run the line, or collect it into a block."""
    global block
    line = input_buffer
    prompt = CONTINUATION if block else PROMPT
    log(prompt + line, C_GREEN)
    remember(line.strip())
    if block:
        if line.strip():
            block.append(line)
            indent = len(line) - len(line.lstrip())
            set_input(" " * (indent + (4 if line.rstrip().endswith(":") else 0)))
            return
        source = "\n".join(block)
        block = []
        set_input("")
        execute_command(source)
        return
    cmd = line.strip()
    if not cmd:
        set_input("")
        return
    if not cmd.startswith("/") and needs_more(line):
        block = [line]
        set_input("    " if line.rstrip().endswith(":") else "")
        return
    set_input("")
    execute_command(cmd)


def cancel_input():
    global block, history_index
    if input_buffer or block:
        log((CONTINUATION if block else PROMPT) + input_buffer + "^C", C_YELLOW)
    block = []
    history_index = len(history)
    set_input("")


def insert(text):
    global input_buffer, cursor
    input_buffer = input_buffer[:cursor] + text + input_buffer[cursor:]
    cursor += len(text)
    refresh_input()


def handle_key(name):
    """A key from a hardware keyboard (see cyd.key())."""
    global input_buffer, cursor, history_index
    if len(name) == 1:
        insert(name)
    elif name == "ENTER":
        submit()
    elif name == "BACKSPACE":
        if cursor > 0:
            input_buffer = input_buffer[:cursor - 1] + input_buffer[cursor:]
            cursor -= 1
            refresh_input()
    elif name == "DELETE":
        if cursor < len(input_buffer):
            input_buffer = input_buffer[:cursor] + input_buffer[cursor + 1:]
            refresh_input()
    elif name == "LEFT":
        set_input(input_buffer, cursor - 1)
    elif name == "RIGHT":
        set_input(input_buffer, cursor + 1)
    elif name == "HOME":
        set_input(input_buffer, 0)
    elif name == "END":
        set_input(input_buffer)
    elif name == "TAB":
        insert(" " * (4 - cursor % 4))
    elif name == "UP":
        if history_index > 0:
            history_index -= 1
            set_input(history[history_index])
    elif name == "DOWN":
        if history_index < len(history):
            history_index += 1
            set_input(history[history_index] if history_index < len(history) else "")
    elif name == "ESC":
        cancel_input()
    elif name == "CTRL+L":
        lines.clear()
        refresh_lines()


def handle_touch_key(ch):
    global kb_mode
    if ch == cyd.KEY_BACKSPACE:
        handle_key("BACKSPACE")
    elif ch == cyd.KEY_CAPS:
        kb_mode = 1 if kb_mode == 0 else 0
        cyd.console_keyboard(kb_mode, -1)
    elif ch == cyd.KEY_SYMBOLS:
        kb_mode = 2 if kb_mode != 2 else 0
        cyd.console_keyboard(kb_mode, -1)
    elif ch == cyd.KEY_ESCAPE:
        cancel_input()
    elif ch == cyd.KEY_ENTER:
        submit()
    else:
        insert(ch)


cyd.no_time_limit()
cyd.title("CONSOLE")
cyd.console_begin()
log("CYD Python Console v1.1", C_CYAN)
log("Type Python or /help. Ctrl+C interrupts.", C_YELLOW)
touch_down_key = -1
hardware_keyboard = cyd.keyboard_connected()
use_hardware_keyboard(hardware_keyboard)

while True:
    try:
        if not cyd.update():
            break
        if cyd.keyboard_connected() != hardware_keyboard:
            hardware_keyboard = not hardware_keyboard
            use_hardware_keyboard(hardware_keyboard)
        while True:
            name = cyd.key()
            if name is None:
                break
            handle_key(name)

        pressed, tx, ty = cyd.touch()
        if hardware_keyboard:
            pass  # no on-screen keys to tap
        elif pressed:
            key = cyd.console_key_at(tx, ty)
            if key != -1 and touch_down_key != key:
                touch_down_key = key
                cyd.console_keyboard(kb_mode, key)
        elif touch_down_key != -1:
            key = touch_down_key
            touch_down_key = -1
            cyd.console_keyboard(kb_mode, -1)
            handle_touch_key(cyd.console_key_char(key, kb_mode))

        time.sleep_ms(30)
    except KeyboardInterrupt:
        # Ctrl+C while waiting for input clears the line, as in a REPL.
        cancel_input()
