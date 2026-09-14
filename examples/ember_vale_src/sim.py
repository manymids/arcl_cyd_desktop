"""Run EMBER VALE on a PC with a stand-in cyd module, to catch crashes.

    python3 examples/ember_vale_src/sim.py [frames] [--shots DIR]

A bot presses A on the title, walks out of the village gate into the fields
and answers every battle with A. --shots writes a PNG every 30 frames, drawn
the way the firmware draws (map, sprites, rectangles, text).
"""

import os
import re
import sys
import types

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
OUT = os.path.join(ROOT, "examples", "ember_vale")
sys.path.insert(0, HERE)

micropython = types.ModuleType("micropython")
micropython.const = lambda value: value
sys.modules["micropython"] = micropython

import random as _random  # noqa: E402
_random.getrandbits  # CPython has it too
import time as _time  # noqa: E402
_clock = [0]
_time.ticks_ms = lambda: _clock[0]
_time.ticks_diff = lambda a, b: a - b

# Pad positions the bot touches.
TOUCH = {"A": (288, 175), "B": (229, 183), 0: (60, 196), 1: (60, 136), 2: (22, 166), 3: (98, 166)}


class FakeCyd(types.ModuleType):
    def __init__(self):
        super().__init__("cyd")
        self.frames = 0
        self.limit = 2000
        self.scene = None
        self.scroll = (0, 0)
        self.sprites = []
        self.rects = [None] * 8
        self.texts = [None] * 16
        self.touching = None
        self.shots = None
        self.battles = 0

    def update(self):
        return self.frames < self.limit

    def no_time_limit(self):
        pass

    def touch(self):
        if self.touching is None:
            return (False, 0, 0)
        x, y = TOUCH[self.touching]
        return (True, x, y)

    def tile_begin(self, palette, tiles, sprites, tilemap, width, height, origin_y=0):
        assert len(palette) % 32 == 0 and len(tiles) % 128 == 0 and len(sprites) % 128 == 0
        assert len(tilemap) >= width * height and height <= 255
        self.scene = [bytes(palette), tiles, sprites, tilemap, width, height, origin_y]
        return True

    def tile_palette(self, palette):
        assert len(palette) == len(self.scene[0])
        self.scene[0] = bytes(palette)

    def tile_frame(self, scroll_x, scroll_y=0):
        self.scroll = (scroll_x, scroll_y)
        self.sprites = []

    def tile_sprite(self, cell, x, y, flags=0):
        assert 0 <= cell < len(self.scene[2]) // 128, cell
        assert len(self.sprites) < 64
        self.sprites.append((cell, x, y, flags))
        return True

    def tile_rect(self, slot, x, y, width, height, fill=0, border=-1):
        assert 0 <= slot < 8
        self.rects[slot] = (x, y, width, height, fill, border) if width > 0 and height > 0 else None
        return True

    def tile_text(self, slot, x, y, value, color=1, scale=1):
        assert 0 <= slot < 16 and len(value) <= 32, (slot, value)
        self.texts[slot] = (x, y, value, color, scale) if value else None
        return True

    def tile_present(self):
        self.frames += 1
        _clock[0] += 40
        bot(self)
        if self.shots and self.frames % 30 == 0:
            shot(self, os.path.join(self.shots, "frame%05d.png" % self.frames))
        return True


def bot(fake):
    import game
    phase = fake.frames % 24
    if game.message_pages:
        fake.touching = "A" if phase < 4 else None
    elif fake.rects[6] is not None:      # a command window: fight
        fake.touching = "A" if phase < 4 else None
    elif fake.frames < 40:
        fake.touching = "A" if phase < 4 else None
    else:
        # Out of the gate, then wander the fields east and south.
        route = (3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 1, 1, 2, 2, 0, 0)
        fake.touching = route[(fake.frames // 40) % len(route)]


_font = None


def font():
    global _font
    if _font is None:
        text = open(os.path.join(ROOT, "firmware", "cyd_desktop_shell", "include", "ui_font.h")).read()
        values = [int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]{2})", text)]
        _font = [values[i * 5:i * 5 + 5] for i in range(95)]
    return _font


def shot(fake, path):
    from build import write_png
    palette, tiles, sprites, tilemap, width, height, origin_y = fake.scene
    colors = []
    for index in range(0, len(palette), 2):
        value = palette[index] << 8 | palette[index + 1]
        colors.append(((value >> 11) << 3, ((value >> 5) & 63) << 2, (value & 31) << 3))

    def nib(data, cell, x, y):
        byte = data[cell * 128 + y * 8 + (x >> 1)]
        return (byte & 15) if x & 1 else byte >> 4

    sx0, sy0 = fake.scroll
    image = [bytearray(320 * 3) for _ in range(240)]
    for y in range(240):
        my = y - origin_y + sy0
        for x in range(320):
            wx = x + sx0
            c = 0
            if 0 <= my < height * 16 and 0 <= wx < width * 16:
                t = tilemap[(my >> 4) * width + (wx >> 4)]
                if t:
                    c = nib(tiles, t, wx & 15, my & 15)
            image[y][x * 3:x * 3 + 3] = bytes(colors[c])
    for cell, x, y, flags in fake.sprites:
        pal = (flags >> 4) % (len(colors) // 16)
        for cy in range(16):
            for cx in range(16):
                px, py = x + cx, y + cy
                if 0 <= px < 320 and 0 <= py < 240:
                    c = nib(sprites, cell, 15 - cx if flags & 1 else cx, 15 - cy if flags & 2 else cy)
                    if c:
                        image[py][px * 3:px * 3 + 3] = bytes(colors[pal * 16 + c])
    for rect in fake.rects:
        if not rect:
            continue
        x, y, w, h, fill, border = rect
        for py in range(max(0, y), min(240, y + h)):
            for px in range(max(0, x), min(320, x + w)):
                edge = border >= 0 and (py - y < 2 or y + h - 1 - py < 2 or px - x < 2 or x + w - 1 - px < 2)
                image[py][px * 3:px * 3 + 3] = bytes(colors[border if edge else fill])
    for entry in fake.texts:
        if not entry:
            continue
        x, y, value, color, scale = entry
        for n, ch in enumerate(value):
            columns = font()[ord(ch) - 32]
            for col in range(5):
                for row in range(7):
                    if columns[col] >> row & 1:
                        for dy in range(scale):
                            for dx in range(scale):
                                px, py = x + (n * 6 + col) * scale + dx, y + row * scale + dy
                                if 0 <= px < 320 and 0 <= py < 240:
                                    image[py][px * 3:px * 3 + 3] = bytes(colors[color])
    write_png(path, 320, 240, image)


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
    original = game.battle

    def counted(number):
        fake.battles += 1
        return original(number)
    game.battle = counted
    game.main()
    print("frames %d, battles %d, level %d, exp %d, gold %d, hp %d/%d, hero (%d, %d)" % (
        fake.frames, fake.battles, game.level, game.exp, game.gold, game.hp, game.max_hp,
        game.hero_x >> 4, game.hero_y >> 4))


if __name__ == "__main__":
    main()
