# A card is optional.  Never format it and never prevent the desktop from
# starting when it is absent or not FAT formatted.
import machine
import os
from _cyd_shell import (app_begin, app_log, app_update, package_next, package_result,
                         app_catalog_add, app_catalog_clear, app_catalog_count,
                         editor_set_text, editor_set_text_ex, editor_set_file_list,
                         runtime_begin, runtime_end, runtime_fail, sd_boot_result,
                         shortcut_clear, shortcut_count, shortcut_remove, shortcut_set,
                         app_error, app_title, console_begin, console_line, console_options,
                         ui_refresh_async)
from _manifest import load as _load_manifest, safe_component as _safe_component, \
    safe_title as _safe_title

# Must match editor_buffer in screen_editor.cpp: 1024 bytes including the terminator.
_EDITOR_CAPACITY = 1020


def _shortcut_path(shortcut_id):
    return "/sd/desktop/shortcuts/" + shortcut_id + ".json"

def _load_shortcuts():
    shortcut_clear()
    try:
        entries = os.listdir("/sd/desktop/shortcuts")
    except OSError:
        return
    for entry in entries:
        if not entry.endswith(".json") or shortcut_count() >= 18:
            continue
        try:
            with open("/sd/desktop/shortcuts/" + entry, "r") as source:
                item = __import__("json").loads(source.read(2048))
            shortcut_id = item.get("id")
            app_id = item.get("app_id")
            title = item.get("title")
            if _safe_component(shortcut_id) and _safe_component(app_id) and _safe_title(title):
                shortcut_set(shortcut_id, app_id, title)
        except Exception:
            pass

def _load_apps():
    app_catalog_clear()
    try:
        entries = sorted(os.listdir("/sd/apps"))
    except OSError:
        return
    for app_id in entries:
        if app_catalog_count() >= 24 or not _safe_component(app_id):
            continue
        try:
            title, entry = _load_manifest(app_id)
        except ValueError as error:
            # Say why an app is missing from Scripts instead of hiding it.
            app_log("skip %s: %s" % (app_id, error))
            continue
        try:
            os.stat("/sd/apps/" + app_id + "/" + entry)
        except OSError:
            continue
        app_catalog_add(app_id, title)

try:
    _desktop_sd = machine.SDCard(slot=2)
    os.mount(_desktop_sd, "/sd")
    for _directory in ("/sd/apps", "/sd/desktop", "/sd/desktop/shortcuts"):
        try:
            os.mkdir(_directory)
        except OSError:
            pass
    _load_shortcuts()
    _load_apps()
    sd_boot_result(0)
except OSError as _error:
    sd_boot_result(_error.args[0] if _error.args else -1)
except Exception:
    sd_boot_result(-2)

_upload = None

def _record_app_error(app_id, error):
    """Keep the whole traceback: the app log line holds only 48 characters."""
    name = type(error).__name__
    app_log("ERROR: %s: %s" % (name, error))
    try:
        import io
        import sys
        buffer = io.StringIO()
        sys.print_exception(error, buffer)
        text = buffer.getvalue()
    except Exception:
        text = "%s: %s\n" % (name, error)
    # desktop_app_error holds 1023 bytes; keep the tail, where the innermost
    # frame and the message are.
    app_error(app_id, text[-1023:])
    result = text
    try:
        _ensure_directory("/sd/desktop")
        _ensure_directory("/sd/desktop/errors")
        with open("/sd/desktop/errors/" + app_id + ".txt", "w") as output:
            output.write(text)
    except Exception:
        pass
    return result

class _Output:
    """print() output and errors of an app that draws nothing itself.

    Shown in the console surface without keyboard or input line. Once the app
    draws its own UI, print() goes to the app's status line instead.
    """
    LINES = 22
    COLUMNS = 52
    WHITE = 0xFFFF
    RED = 0xF800
    YELLOW = 0xFFE0

    def __init__(self, title):
        self.title = title
        self.lines = []
        self.shown = False
        self.partial = ""

    def write(self, text, color=WHITE):
        import cyd
        if not self.shown or cyd._ui_used:
            console_begin()
            console_options(False, False)
            app_title(self.title)
            self.shown = True
            cyd._ui_used = False
            self.lines = []
        text = self.partial + text
        pieces = text.split("\n")
        self.partial = pieces.pop()
        for piece in pieces:
            self._add(piece, color)
        self._draw()

    def flush(self, color=WHITE):
        if self.partial:
            self._add(self.partial, color)
            self.partial = ""

    def _add(self, line, color):
        while len(line) > self.COLUMNS:
            self.lines.append((line[:self.COLUMNS], color))
            line = line[self.COLUMNS:]
        self.lines.append((line, color))
        del self.lines[:-self.LINES]

    def _draw(self):
        for index in range(self.LINES):
            if index < len(self.lines):
                console_line(index, self.lines[index][0], self.lines[index][1])
            else:
                console_line(index, "", self.WHITE)
        ui_refresh_async()

def _screen_print(output):
    def screen_print(*args, sep=" ", end="\n", file=None):
        text = sep.join([str(value) for value in args]) + end
        if file is not None:
            file.write(text)
            return
        import cyd
        if cyd._ui_used:
            # The app owns the screen: keep the latest line in its status bar.
            line = text.rstrip("\n").split("\n")[-1]
            if line:
                app_log(line[-48:])
            return
        output.write(text)
    return screen_print

def _run_app(app_id):
    app_begin(app_id)
    if not runtime_begin():
        app_log("Runtime unavailable")
        runtime_fail()
        return
    output = None
    try:
        title, entry = _load_manifest(app_id)
        output = _Output(title)
        path = "/sd/apps/" + app_id + "/" + entry
        with open(path, "rb") as source:
            code = compile(source.read(), path, "exec")
        import cyd
        cyd._set_app_root(app_id)
        namespace = {"__name__": "__main__", "__file__": path, "print": _screen_print(output)}
        exec(code, namespace, namespace)
        app_log("Stopped")
        if output.shown and not cyd._ui_used:
            output.flush()
            output.write("[finished]\n", _Output.YELLOW)
    except SystemExit:
        # sys.exit() is a normal way to finish, not a crash worth a traceback.
        app_log("Stopped")
        runtime_fail()
    except KeyboardInterrupt:
        # Ctrl+C on a hardware keyboard. Keep printed output readable.
        app_log("Stopped: Ctrl+C")
        if output is not None and output.shown:
            output.flush()
            output.write("KeyboardInterrupt\n", _Output.RED)
        else:
            runtime_fail()
    except BaseException as error:
        text = _record_app_error(app_id, error)
        # Leave the traceback on screen until the user goes Home. A video app
        # owns a native surface the console cannot draw over, so it goes Home.
        try:
            import cyd
            if cyd._video_used:
                raise RuntimeError("video surface")
            if output is None:
                output = _Output(app_id)
            output.flush()
            output.write(text, _Output.RED)
            output.write("Tap HOME to close.\n", _Output.YELLOW)
        except BaseException:
            runtime_fail()
    finally:
        # A second Ctrl+C must not skip releasing the watchdog.
        for _attempt in range(3):
            try:
                runtime_end()
                app_update()
                break
            except KeyboardInterrupt:
                pass

def _ensure_directory(path):
    try:
        os.mkdir(path)
    except OSError:
        pass

def _shortcut_write(shortcut_id, app_id, title, require_existing):
    target = _shortcut_path(shortcut_id)
    exists = False
    try:
        os.stat(target)
        exists = True
    except OSError:
        pass
    if require_existing and not exists:
        raise OSError("shortcut does not exist")
    if not require_existing and exists:
        raise OSError("shortcut already exists")
    if not exists and shortcut_count() >= 18:
        raise OSError("desktop shortcut limit reached")
    temporary = target + ".part"
    try:
        os.remove(temporary)
    except OSError:
        pass
    previous = target + ".bak"
    try:
        os.remove(previous)
    except OSError:
        pass
    saved_previous = False
    try:
        with open(temporary, "w") as output:
            output.write(__import__("json").dumps({"id": shortcut_id, "app_id": app_id, "title": title}))
        # Move the old definition aside rather than deleting it, so a power cut
        # between the two renames still leaves one complete file on the card.
        if exists:
            os.rename(target, previous)
            saved_previous = True
        os.rename(temporary, target)
        if saved_previous:
            try:
                os.remove(previous)
            except OSError:
                pass
        if not shortcut_set(shortcut_id, app_id, title):
            raise OSError("desktop shortcut limit reached")
    except Exception:
        try:
            os.remove(temporary)
        except OSError:
            pass
        # Put the previous definition back if it was moved but not replaced.
        if saved_previous:
            try:
                os.stat(target)
            except OSError:
                try:
                    os.rename(previous, target)
                except OSError:
                    pass
        raise

def _shortcut_delete(shortcut_id):
    target = _shortcut_path(shortcut_id)
    try:
        os.remove(target)
    except OSError:
        raise OSError("shortcut does not exist")
    shortcut_remove(shortcut_id)

def _remove_tree(path):
    for entry in os.listdir(path):
        child = path + "/" + entry
        try:
            if os.stat(child)[0] & 0x4000:
                _remove_tree(child)
                os.rmdir(child)
            else:
                os.remove(child)
        except OSError:
            raise

def _app_delete(app_id):
    if not _safe_component(app_id):
        raise OSError("invalid app id")
    target = "/sd/apps/" + app_id
    try:
        _remove_tree(target)
        os.rmdir(target)
    except OSError:
        raise OSError("app delete failed")
    try:
        entries = os.listdir("/sd/desktop/shortcuts")
    except OSError:
        entries = ()
    for entry in entries:
        if not entry.endswith(".json"):
            continue
        path = "/sd/desktop/shortcuts/" + entry
        try:
            with open(path, "r") as source:
                item = __import__("json").loads(source.read())
            if item.get("app_id") == app_id:
                os.remove(path)
        except Exception:
            pass
    _load_shortcuts()
    _load_apps()

def _package_worker():
    while True:
        try:
            _package_step()
        except KeyboardInterrupt:
            # Ctrl+C meant for an app that had just finished.
            pass

def _package_step():
    global _upload
    message = package_next()
    if message is None:
        return
    operation, app_id, filename, payload, expected_size, expected_sha, reply_requested = message
    if operation == "run":
        if reply_requested:
            package_result(True, "")
        _run_app(app_id)
        return
    if operation == "apps_rescan":
        _load_apps()
        ui_refresh_async()
        return
    try:
        if operation == "begin" or operation == "begin_replace":
            if _upload is not None:
                raise OSError("another upload is active")
            _ensure_directory("/sd/apps")
            _ensure_directory("/sd/apps/" + app_id)
            target = "/sd/apps/" + app_id + "/" + filename
            temporary = target + ".part"
            try:
                os.remove(temporary)
            except OSError:
                pass
            output = open(temporary, "wb")
            _upload = (target, temporary, expected_size, expected_sha, 0, output,
                       operation == "begin_replace")
        elif operation == "chunk":
            if _upload is None:
                raise OSError("no active upload")
            data = __import__("ubinascii").a2b_base64(payload)
            target, temporary, expected_size, expected_sha, written, output, replace = _upload
            if written + len(data) > expected_size:
                raise OSError("chunk exceeds declared size")
            output.write(data)
            _upload = (target, temporary, expected_size, expected_sha, written + len(data), output, replace)
        elif operation == "finish":
            if _upload is None:
                raise OSError("no active upload")
            target, temporary, expected_size, expected_sha, written, output, replace = _upload
            if written != expected_size:
                raise OSError("size mismatch")
            output.close()
            digest = __import__("hashlib").sha256()
            with open(temporary, "rb") as source:
                while True:
                    block = source.read(4096)
                    if not block:
                        break
                    digest.update(block)
            actual_sha = __import__("ubinascii").hexlify(digest.digest()).decode()
            if actual_sha != expected_sha:
                raise OSError("sha256 mismatch")
            target_exists = False
            try:
                os.stat(target)
                target_exists = True
            except OSError:
                pass
            if target_exists and not replace:
                raise OSError("target already exists")
            if target_exists:
                os.remove(target)
            os.rename(temporary, target)
            _upload = None
            _load_apps()
        elif operation == "shortcut_create":
            _shortcut_write(app_id, filename, payload, False)
        elif operation == "shortcut_update":
            _shortcut_write(app_id, filename, payload, True)
        elif operation == "shortcut_delete":
            _shortcut_delete(app_id)
        elif operation == "app_delete":
            _app_delete(app_id)
        elif operation == "file_read":
            try:
                info = os.stat(app_id)
                if info[0] & 0x4000:
                    raise OSError("cannot open directory")
                with open(app_id, "r") as source:
                    # Read one byte past what the editor can hold so a file
                    # that does not fit is detected rather than silently
                    # cut. Saving a truncated buffer would delete the rest
                    # of the file, so the editor refuses it.
                    text = source.read(_EDITOR_CAPACITY + 1)
                truncated = len(text) > _EDITOR_CAPACITY
                editor_set_text_ex(text[:_EDITOR_CAPACITY], truncated)
            except Exception as error:
                editor_set_text_ex("", False)
                raise OSError("file_read failed: " + str(error))
        elif operation == "file_write":
            with open(app_id, "w") as target:
                target.write(payload)
                try:
                    target.flush()
                except Exception:
                    pass
            try:
                os.sync()
            except Exception:
                pass
        elif operation == "file_list":
            try:
                directory = app_id if app_id else "/sd"
                files = []
                try:
                    for entry in os.ilistdir(directory):
                        name = entry[0]
                        entry_type = entry[1]
                        if entry_type == 0x8000:
                            files.append(name)
                except Exception:
                    for name in os.listdir(directory):
                        try:
                            if not (os.stat(directory + "/" + name)[0] & 0x4000):
                                files.append(name)
                        except Exception:
                            pass
                text_exts = (".json", ".py", ".txt", ".ini", ".cfg", ".conf", ".log", ".csv", ".md")
                txt = sorted([f for f in files if any(f.lower().endswith(ext) for ext in text_exts)])
                other = sorted([f for f in files if not any(f.lower().endswith(ext) for ext in text_exts)])
                ordered = txt + other
                editor_set_file_list("\n".join(ordered[:24]))
            except Exception:
                editor_set_file_list("")
        else:
            raise OSError("unknown package operation")
        if reply_requested:
            package_result(True, "")
    except Exception as error:
        if _upload is not None:
            try:
                _upload[5].close()
            except Exception:
                pass
            try:
                os.remove(_upload[1])
            except OSError:
                pass
            _upload = None
        if reply_requested:
            package_result(False, str(error))

_package_worker()
