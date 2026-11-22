"""Host test for the IS01 / 12HT-compatible helpers in cyd.py (API level 7).

Runs cyd.py under CPython against a stand-in _cyd_shell, so the Python logic
(touch edges, key edges, packed sprite records, features) is checked without
the board.
"""

import os
import shutil
import tempfile
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
        self.images = {}
        self.overlays = []
        self.scene = None
        self.scene_images = {}
        self.scene_overlays = []
        self.presented = []

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

    def game_image_set(self, slot, pixels, width, height):
        if pixels is None:
            self.images.pop(slot, None)
        else:
            assert len(pixels) == width * height * 2
            self.images[slot] = (bytes(pixels), width, height)
        return True

    def overlay_add(self, *args):
        self.overlays.append(args)
        return True

    def scene_begin(self):
        self.scene = "front"
        return 0

    def scene_end(self):
        self.scene = None
        self.scene_images = {}

    def scene_image(self, slot, pixels, width, height):
        if pixels is None:
            self.scene_images.pop(slot, None)
        else:
            self.scene_images[slot] = (pixels, width, height)
        return True

    def scene_overlay_add(self, *args):
        self.scene_overlays.append(args)
        return True

    def scene_present(self, records, background):
        if self.scene != "front":
            return -4
        if len(records) % 22:
            return -2
        self.presented.append((bytes(records), background))
        return len(records) // 22

    def buffer_copy(self, destination, source):
        assert len(destination) == len(source)
        destination[:] = source


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
    assert {"keyboard", "phys", "tile", "touch_points", "video"} <= set(names)
    assert "wifi" not in names
    shell.radio = "wifi"
    assert "wifi" in cyd.features()
    assert cyd._report(False, "no file") is False and cyd.last_error() == "no file"
    assert cyd._report(True, "") is True and cyd.last_error() is None


def test_load_image():
    folder = tempfile.mkdtemp()
    try:
        cyd._set_app_root("imgtest")
        cyd._app_root = folder + "/data"
        with open(os.path.join(folder, "ship.rgb"), "wb") as target:
            target.write(struct.pack("<4H", 0x0000, 0xF800, 0x07E0, 0x001F))
        with open(os.path.join(folder, "short.rgb"), "wb") as target:
            target.write(bytes(6))
        assert cyd.load_image("ship.rgb", 2, 2) == 0
        assert shell.images[0] == (struct.pack("<4H", 0, 0xF800, 0x07E0, 0x001F), 2, 2)
        assert cyd.last_error() is None
        assert cyd.load_image("ship.rgb", 2, 2) == 1, "each load takes the next slot"
        for bad in [("short.rgb", 2, 2), ("ship.rgb", 1, 2), ("../ship.rgb", 2, 2), ("ship.rgb", 0, 2),
                    ("missing.rgb", 2, 2)]:
            try:
                cyd.load_image(*bad)
            except (ValueError, OSError):
                assert cyd.last_error(), bad
            else:
                raise AssertionError("accepted %r" % (bad,))
        for slot in range(2, cyd.IMAGE_CAP):
            cyd.load_image("ship.rgb", 2, 2)
        try:
            cyd.load_image("ship.rgb", 2, 2)
        except MemoryError:
            pass
        else:
            raise AssertionError("more than IMAGE_CAP images")
        cyd._set_app_root("next")
        assert shell.images == {}, "images are released at the next app start"
        assert cyd._images == [None] * cyd.IMAGE_CAP
    finally:
        shutil.rmtree(folder)


def test_overlay():
    shell.overlays = []
    cyd.game_begin()
    assert cyd.overlay_text(160, 10, "HI", cyd.YELLOW, 2, align=cyd.ALIGN_CENTER)
    assert cyd.overlay_rect(0, 0, 10, 5, cyd.RED, fill=False)
    assert shell.overlays[0] == (0, 160, 10, 0, 0, cyd.YELLOW, "HI", 2, 0x0821, 1)
    assert shell.overlays[1] == (2, 0, 0, 10, 5, cyd.RED, None, 1, 0, 0)
    names = cyd.features()
    assert "image" in names and "overlay" in names


def test_sprite_scene():
    cyd._set_app_root("scenetest")
    shell.scene_overlays = []
    shell.presented = []
    solid = struct.pack("<4H", 0xF800, 0xF800, 0xF800, 0xF800)
    first = cyd.sprite_create(2, 2, solid)
    blank = cyd.sprite_create(3, 1)
    assert (first, blank) == (0, 1)
    assert bytes(shell.scene_images[0][0]) == solid and bytes(shell.scene_images[1][0]) == bytes(6)
    cyd.sprite_update(blank, struct.pack("<3H", 1, 2, 3))
    assert bytes(shell.scene_images[1][0]) == struct.pack("<3H", 1, 2, 3)
    for bad in [lambda: cyd.sprite_update(5, bytes(2)), lambda: cyd.sprite_update(blank, bytes(2)),
                lambda: cyd.sprite_create(0, 2)]:
        try:
            bad()
        except (ValueError, AssertionError):
            pass
        else:
            raise AssertionError("accepted a bad sprite call")
    # Outside game mode the overlay goes to the scene, which it enters.
    assert cyd.overlay_text(4, 4, "SCORE") and shell.scene == "front"
    record = struct.pack("<HhhHHHhhHBBH", first, 10, 10, 256, 256, 0, 1, 1, 0, 255, 0, 0)
    assert cyd.sprite_present(record + record, cyd.NAVY) == 2
    assert shell.presented[-1] == (record + record, cyd.NAVY)
    assert shell.scene_overlays[0][6] == "SCORE"
    try:
        cyd.sprite_present(bytes(5))
    except ValueError:
        pass
    else:
        raise AssertionError("a partial record must be refused")
    shell.scene = "left"
    assert cyd.sprite_present(record) == -1, "the view has left the screen"
    assert "sprite" in cyd.features()
    cyd._set_app_root("next")
    assert shell.scene is None and cyd._sprites == [None] * cyd.SPRITE_CAP and not cyd._scene_used


def main():
    for name, test in sorted(globals().items()):
        if name.startswith("test_"):
            test()
    print("cyd_compat: ok")


if __name__ == "__main__":
    main()
