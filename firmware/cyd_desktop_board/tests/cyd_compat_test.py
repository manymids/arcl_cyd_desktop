"""Host test for the IS01 / 12HT-compatible helpers in cyd.py (API level 7).

Runs cyd.py under CPython against a stand-in _cyd_shell, so the Python logic
(touch edges, key edges, packed sprite records, features) is checked without
the board.
"""

import os
import struct
import sys
import types

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "modules"))


class Shell:
    """What the native _cyd_shell module would report."""

    def __init__(self):
        self.touch = (False, 0, 0)
        self.keys = ()
        self.sprites = []
        self.capacity = 3
        self.dropped = 0
        self.radio = "off"

    def game_touch(self):
        return self.touch

    def keys_down(self):
        return self.keys

    def game_sprite(self, kind, x, y, color, size):
        if len(self.sprites) >= self.capacity:
            self.dropped += 1
            return False
        self.sprites.append((kind, x, y, color, size))
        return True

    def game_draw_stats(self):
        return (len(self.sprites), 0, self.dropped, 0)

    def radio_mode(self):
        return self.radio


shell = Shell()
fake = types.ModuleType("_cyd_shell")


def _lookup(name):
    if hasattr(shell, name):
        return getattr(shell, name)
    return lambda *args, **kwargs: None


fake.__getattr__ = _lookup
sys.modules["_cyd_shell"] = fake

import cyd  # noqa: E402


def test_touch():
    shell.touch = (False, 0, 0)
    assert cyd.touch_pressed() is None
    assert cyd.touch_points() == ()
    assert cyd.touch_edge() is None
    shell.touch = (True, 10, 20)
    assert cyd.touch_pressed() == (10, 20)
    assert cyd.touch_points() == ((10, 20, 0),)
    assert cyd.touch_edge() == (10, 20), "the press start is an edge"
    assert cyd.touch_edge() is None, "holding is not a new edge"
    shell.touch = (False, 0, 0)
    assert cyd.touch_edge() is None
    shell.touch = (True, 5, 6)
    assert cyd.touch_edge() == (5, 6), "a second press is a new edge"


def test_key_state():
    shell.keys = ()
    assert cyd.key_state("w") == 0
    shell.keys = ("w",)
    assert cyd.key_state("w", clear=False) == cyd.KEY_DOWN | cyd.KEY_PRESSED
    assert cyd.key_state("w") == cyd.KEY_DOWN | cyd.KEY_PRESSED, "clear=False kept the edge"
    assert cyd.key_state("w") == cyd.KEY_DOWN, "the edge was consumed"
    shell.keys = ()
    assert cyd.key_state("w") == cyd.KEY_RELEASED
    assert cyd.key_state("w") == 0
    # A tap that starts and ends between two key_state calls is still seen,
    # because update() polls once key_state has been used.
    shell.keys = ("LEFT",)
    cyd.update()
    shell.keys = ()
    cyd.update()
    assert cyd.key_state("LEFT") == cyd.KEY_PRESSED | cyd.KEY_RELEASED


def test_app_start_forgets_edges():
    shell.keys = ("a",)
    cyd.key_state("z")  # start tracking; "a" now has a pending press edge
    cyd._set_app_root("next")
    shell.keys = ()
    assert cyd.key_state("a") == 0, "the previous app's edges were dropped"
    assert cyd.last_error() is None


def test_game_sprites():
    shell.sprites = []
    shell.dropped = 0
    records = bytearray()
    for index in range(5):
        records += struct.pack("<BhhHB", 1, -3 + index, 200, 0xF800, 4)
    assert cyd.game_sprites(records) == 3, "capacity 3 of 5"
    assert shell.sprites[0] == (1, -3, 200, 0xF800, 4)
    assert cyd.game_draw_stats() == (3, 0, 2, 0)
    try:
        cyd.game_sprites(b"\x00" * 7)
    except ValueError:
        pass
    else:
        raise AssertionError("a partial record must be refused")


def test_misc():
    assert cyd.screen_size() == (320, 240)
    assert abs(cyd.hypot(3, 4) - 5.0) < 1e-9
    shell.radio = "off"
    names = cyd.features()
    assert names == tuple(sorted(names))
    assert {"keyboard", "tile", "touch_points", "video"} <= set(names)
    assert "wifi" not in names
    shell.radio = "wifi"
    assert "wifi" in cyd.features()
    assert cyd._report(False, "no file") is False and cyd.last_error() == "no file"
    assert cyd._report(True, "") is True and cyd.last_error() is None


def main():
    for name, test in sorted(globals().items()):
        if name.startswith("test_"):
            test()
    print("cyd_compat: ok")


if __name__ == "__main__":
    main()
