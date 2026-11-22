"""Run PIP QUEST on a PC with a stand-in cyd module, to catch crashes.

    python3 examples/pip_quest_src/sim.py [frames] [--shots DIR]

A simple bot holds right and dashes, jumping when something is ahead. --shots
writes a PNG of the screen every 120 frames, drawn like the firmware does.
"""

import os
import sys
import types

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "pip_quest")
sys.path.insert(0, HERE)

micropython = types.ModuleType("micropython")
micropython.const = lambda value: value
sys.modules["micropython"] = micropython

import time as _time  # noqa: E402
_clock = [0]
_time.ticks_ms = lambda: _clock[0]
_time.ticks_diff = lambda a, b: a - b


class FakeCyd(types.ModuleType):
    def __init__(self):
        super().__init__("cyd")
        self.frames = 0
        self.limit = 3000
        self.held = ()
        self.queue = []
        self.scene = None
        self.texts = [None] * 8
        self.sprites = []
        self.scroll = 0
        self.shots = None
        self.storage = {}

    def update(self):
        return self.frames < self.limit

    def no_time_limit(self):
        pass

    def keys_down(self):
        return self.held

    def key(self):
        return self.queue.pop(0) if self.queue else None

    def storage_get(self, key, default=None):
        return self.storage.get(key, default)

    def storage_put(self, key, value):
        self.storage[key] = value

    def tile_begin(self, palette, tiles, sprites, tilemap, width, height, origin_y=0):
        assert len(palette) % 32 == 0 and len(tiles) % 128 == 0 and len(sprites) % 128 == 0
        assert len(tilemap) >= width * height
        self.scene = (bytes(palette), tiles, sprites, tilemap, width, height, origin_y)
        return True

    def tile_frame(self, scroll_x):
        self.scroll = scroll_x
        self.sprites = []

    def tile_sprite(self, cell, x, y, flags=0):
        assert 0 <= cell < len(self.scene[2]) // 128, cell
        assert len(self.sprites) < 64, "too many sprites"
        self.sprites.append((cell, x, y, flags))
        return True

    def tile_text(self, slot, x, y, value, color=1, scale=1):
        assert 0 <= slot < 8 and len(value) <= 32, value
        self.texts[slot] = (x, y, value, color, scale) if value else None
        return True

    def tile_present(self):
        self.frames += 1
        _clock[0] += 33
        bot(self)
        if self.shots and self.frames % 40 == 0:
            shot(self, os.path.join(self.shots, "frame%05d.png" % self.frames))
        return True


def bot(fake):
    import game
    if fake.frames % 200 == 1 and game.in_title:
        fake.queue.append("ENTER")
    if game.tilemap is None:
        fake.held = ()
        return
    x = (game.px >> 12) + 16
    y = (game.py >> 12) + game.player_height() - 8
    ahead = game.solid_at(x + 8, y) or game.tile_at((x + 24) >> 4, 11) == 0 or any(
        0 < (e[1] >> 12) - x < 40 for e in game.enemies)
    phase = fake.frames % 40
    keys = ["RIGHT", "x"]
    if ahead and phase < 20:
        keys.append("z")
    fake.held = tuple(keys)


def shot(fake, path):
    import build
    import art
    palette, tiles, sprites, tilemap, width, height, origin_y = fake.scene
    colors = []
    for index in range(0, len(palette), 2):
        value = palette[index] << 8 | palette[index + 1]
        colors.append(((value >> 11) << 3, ((value >> 5) & 63) << 2, (value & 31) << 3))
    def nib(data, cell, x, y):
        byte = data[cell * 128 + y * 8 + (x >> 1)]
        return (byte & 15) if x & 1 else byte >> 4
    image = [bytearray(320 * 3) for _ in range(240)]
    index_rows = [bytearray(320) for _ in range(240)]
    for sy in range(240):
        my = sy - origin_y
        for sx in range(320):
            wx = sx + fake.scroll
            c = 0
            if 0 <= my < height * 16 and 0 <= wx < width * 16:
                t = tilemap[(my >> 4) * width + (wx >> 4)]
                if t:
                    c = nib(tiles, t, wx & 15, my & 15)
            index_rows[sy][sx] = c
            image[sy][sx * 3:sx * 3 + 3] = bytes(colors[c])
    for cell, x, y, flags in fake.sprites:
        pal = (flags >> 4) % (len(colors) // 16)
        for cy in range(16):
            for cx in range(16):
                sx, sy = x + cx, y + cy
                if not (0 <= sx < 320 and 0 <= sy < 240):
                    continue
                c = nib(sprites, cell, 15 - cx if flags & 1 else cx, 15 - cy if flags & 2 else cy)
                if c and not (flags & 4 and index_rows[sy][sx]):
                    image[sy][sx * 3:sx * 3 + 3] = bytes(colors[pal * 16 + c])
    sys.path.insert(0, os.path.join(HERE, "..", "..", "firmware"))
    font = load_font()
    for entry in fake.texts:
        if not entry:
            continue
        x, y, value, color, scale = entry
        for n, ch in enumerate(value):
            cols = font[ord(ch) - 32] if 32 <= ord(ch) <= 126 else font[31]
            for col in range(5):
                for row in range(7):
                    if cols[col] >> row & 1:
                        for dy in range(scale):
                            for dx in range(scale):
                                px, py = x + n * 6 * scale + col * scale + dx, y + row * scale + dy
                                if 0 <= px < 320 and 0 <= py < 240:
                                    image[py][px * 3:px * 3 + 3] = bytes(colors[color])
    build.write_png(path, 320, 240, image)


_font = None


def load_font():
    global _font
    if _font is None:
        import re
        text = open(os.path.join(HERE, "..", "..", "firmware", "cyd_desktop_shell", "include", "ui_font.h")).read()
        values = [int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]{2})", text)]
        _font = [values[i * 5:i * 5 + 5] for i in range(95)]
    return _font


def main():
    fake = FakeCyd()
    sys.modules["cyd"] = fake
    if len(sys.argv) > 1 and sys.argv[1].isdigit():
        fake.limit = int(sys.argv[1])
    if "--shots" in sys.argv:
        fake.shots = sys.argv[sys.argv.index("--shots") + 1]
        os.makedirs(fake.shots, exist_ok=True)
    import game
    game.APP_DIR = OUT + "/"
    game.in_title = False
    title = game.title_screen
    def titled():
        game.in_title = True
        title()
        game.in_title = False
    game.title_screen = titled
    if "--warp" in sys.argv:
        # Start every stage a few tiles before its goal.
        original = game.start_stage
        def warped():
            original()
            column = game.goal_column - 12
            game.px = column * 16 << 12
            game.py = 0
            game.camera = column * 16 - 100
            game.next_object = game.object_count
        game.start_stage = warped
    game.main()
    print("frames %d, stage %d, score %d, lives %d, x %d, state %d" % (
        fake.frames, game.stage_number, game.score, game.lives, game.px >> 12, game.state))


if __name__ == "__main__":
    main()
