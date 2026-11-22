"""Build EMBER VALE into examples/ember_vale.

    python3 examples/ember_vale_src/build.py [--preview DIR]

Writes gfx.bin, world.bin and game.mpy (compiled with mpy-cross, so the device
does not compile the game in its heap). --preview writes PNG sheets of the
cells and the whole world map.
"""

import os
import re
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "examples", "ember_vale")
sys.path.insert(0, HERE)

import art  # noqa: E402
import world  # noqa: E402


def check_cell(name, rows):
    assert len(rows) == 16, "%s: %d rows" % (name, len(rows))
    for number, row in enumerate(rows):
        assert len(row) == 16, "%s row %d: %d columns %r" % (name, number, len(row), row)
        for character in row:
            assert character in art.INK, "%s row %d: %r" % (name, number, character)


def pack_cell(rows):
    data = bytearray()
    for row in rows:
        for column in range(0, 16, 2):
            data.append(art.INK[row[column]] << 4 | art.INK[row[column + 1]])
    return data


def pack_palettes(palettes):
    data = bytearray()
    for colors in palettes:
        for red, green, blue in colors:
            data += struct.pack(">H", (red & 0xF8) << 8 | (green & 0xFC) << 3 | blue >> 3)
    return data


def check_ids():
    """game.py names tiles and sprites by number; they must match art.py."""
    source = open(os.path.join(HERE, "game.py"), encoding="utf-8").read()
    for prefix, table in (("T_", art.TILES), ("S_", art.SPRITES)):
        names = [name for name, _ in table]
        for name, value in re.findall(r"^%s(\w+) = const\((\d+)\)" % prefix, source, re.M):
            assert name in names, "game.py %s%s is not in art.py" % (prefix, name)
            assert names.index(name) == int(value), "game.py %s%s = %s, art.py has %d" % (
                prefix, name, value, names.index(name))


def build_gfx():
    for name, rows in art.TILES + art.SPRITES:
        check_cell(name, rows)
    data = bytearray(b"EVG1")
    data += bytes((len(art.TILES), len(art.SPRITES), 0, 0))
    data += pack_palettes(art.PALETTES)
    data += pack_palettes(art.PALETTES_SHIMMER)
    for _, rows in art.TILES + art.SPRITES:
        data += pack_cell(rows)
    return data


def build_world(w):
    data = bytearray(b"EVW1")
    data += bytes((world.WIDTH, world.HEIGHT, w.start[0], w.start[1], len(w.people)))
    data += w.map
    for kind, column, row, wanders, message in w.people:
        data += bytes((kind, column, row, 1 if wanders else 0, message))
    return data


def compile_game():
    source = os.path.join(HERE, "game.py")
    target = os.path.join(OUT, "game.mpy")
    tool = os.path.join(ROOT, "firmware", "micropython", "mpy-cross", "build", "mpy-cross")
    command = [tool, "-o", target, "-s", "game.py", source]
    if os.name == "nt":
        def wsl_path(path):
            drive, rest = os.path.splitdrive(path)
            return "/mnt/%s%s" % (drive[0].lower(), rest.replace("\\", "/"))
        command = ["wsl", "-d", "Ubuntu-24.04", "-e", wsl_path(tool), "-o", wsl_path(target),
                   "-s", "game.py", wsl_path(source)]
    subprocess.check_call(command)
    return os.path.getsize(target)


def write_png(path, width, height, rgb_rows):
    raw = b"".join(b"\x00" + bytes(row) for row in rgb_rows)

    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))
    with open(path, "wb") as output:
        output.write(b"\x89PNG\r\n\x1a\n")
        output.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        output.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        output.write(chunk(b"IEND", b""))


def render_cells(cells, colors, per_row=10, scale=3):
    rows = (len(cells) + per_row - 1) // per_row
    width, height = per_row * 17 * scale, rows * 17 * scale
    image = [bytearray(b"\x30\x60\x30" * width) for _ in range(height)]
    for index, cell in enumerate(cells):
        ox, oy = (index % per_row) * 17 * scale, (index // per_row) * 17 * scale
        for y, row in enumerate(cell):
            for x, character in enumerate(row):
                color = bytes(colors[art.INK[character]])
                for dy in range(scale):
                    for dx in range(scale):
                        offset = (ox + x * scale + dx) * 3
                        image[oy + y * scale + dy][offset:offset + 3] = color
    return width, height, image


def render_world(w):
    tiles = [rows for _, rows in art.TILES]
    width, height = world.WIDTH * 16, world.HEIGHT * 16
    image = [bytearray(width * 3) for _ in range(height)]
    for row in range(world.HEIGHT):
        for column in range(world.WIDTH):
            tile = tiles[w.map[row * world.WIDTH + column]]
            for y in range(16):
                line = image[row * 16 + y]
                for x in range(16):
                    offset = (column * 16 + x) * 3
                    line[offset:offset + 3] = bytes(art.BASE[art.INK[tile[y][x]]])
    sprites = dict(art.SPRITES)
    marks = [("ELDER_1" if kind else "VILLAGER_1", column, row) for kind, column, row, _, _ in w.people]
    marks.append(("HERO_DOWN_1", w.start[0], w.start[1]))
    for name, column, row in marks:
        for y, line in enumerate(sprites[name]):
            for x, character in enumerate(line):
                if character != ".":
                    offset = (column * 16 + x) * 3
                    image[row * 16 + y][offset:offset + 3] = bytes(art.BASE[art.INK[character]])
    return width, height, image


def main():
    check_ids()
    os.makedirs(OUT, exist_ok=True)
    gfx = build_gfx()
    with open(os.path.join(OUT, "gfx.bin"), "wb") as output:
        output.write(gfx)
    print("gfx.bin %d bytes: %d tiles, %d sprites" % (len(gfx), len(art.TILES), len(art.SPRITES)))
    w = world.build()
    data = build_world(w)
    with open(os.path.join(OUT, "world.bin"), "wb") as output:
        output.write(data)
    print("world.bin %d bytes" % len(data))
    print("game.mpy %d bytes" % compile_game())
    if "--preview" in sys.argv:
        directory = sys.argv[sys.argv.index("--preview") + 1]
        os.makedirs(directory, exist_ok=True)
        write_png(os.path.join(directory, "tiles.png"), *render_cells([r for _, r in art.TILES], art.BASE))
        write_png(os.path.join(directory, "sprites.png"), *render_cells([r for _, r in art.SPRITES], art.BASE))
        write_png(os.path.join(directory, "world.png"), *render_world(w))
        print("previews in", directory)


if __name__ == "__main__":
    main()
