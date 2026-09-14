"""Build PIP QUEST into examples/pip_quest.

    python3 examples/pip_quest_src/build.py [--preview DIR]

Writes gfx.bin (palettes and 4-bit cells), stage1.bin.. (tile maps and
objects) and game.mpy (compiled with firmware/micropython/mpy-cross, so the
device does not compile the game in its small heap). --preview also writes
PNG sheets of the cells and whole stages for checking the art on a PC.
"""

import os
import re
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "examples", "pip_quest")
sys.path.insert(0, HERE)

import art  # noqa: E402
import levels  # noqa: E402


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


def pack_palette(colors):
    data = bytearray()
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
    data = bytearray(b"PQG1")
    data += bytes((len(art.THEMES), len(art.TILES), len(art.SPRITES), 0))
    for scenery in art.THEMES:
        for colors in (scenery, art.CHARACTER, art.EMBER_FORM, art.COMET_FORM):
            data += pack_palette(colors)
    for _, rows in art.TILES:
        data += pack_cell(rows)
    for _, rows in art.SPRITES:
        data += pack_cell(rows)
    return data


def build_stage(stage):
    assert stage.width * levels.ROWS <= 2600, "%s: wider than game.MAP_CAPACITY" % stage.name
    data = bytearray(b"PQS1")
    data += struct.pack("<HBHHHB", stage.width, stage.theme, stage.time, stage.goal,
                        stage.checkpoint, len(stage.objects))
    data += stage.name.encode()[:3].ljust(3)
    data += stage.map
    for kind, column, row in sorted(stage.objects, key=lambda item: item[1]):
        assert column < 256
        data += bytes((kind, column, row))
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


# ---------------------------------------------------------------------------
# Previews

def write_png(path, width, height, rgb_rows):
    raw = b"".join(b"\x00" + bytes(row) for row in rgb_rows)
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
    with open(path, "wb") as output:
        output.write(b"\x89PNG\r\n\x1a\n")
        output.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        output.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        output.write(chunk(b"IEND", b""))


def render_cells(cells, colors, per_row=12, scale=3):
    rows_of_cells = (len(cells) + per_row - 1) // per_row
    width = per_row * 17 * scale
    height = rows_of_cells * 17 * scale
    image = [bytearray(b"\x40\x40\x40" * width) for _ in range(height)]
    for index, rows in enumerate(cells):
        cell_x = (index % per_row) * 17 * scale
        cell_y = (index // per_row) * 17 * scale
        for y, row in enumerate(rows):
            for x, character in enumerate(row):
                red, green, blue = colors[art.INK[character]]
                for dy in range(scale):
                    line = image[cell_y + y * scale + dy]
                    for dx in range(scale):
                        offset = (cell_x + x * scale + dx) * 3
                        line[offset:offset + 3] = bytes((red, green, blue))
    return width, height, image


def render_stage(stage, sprites_by_name):
    colors = art.THEMES[stage.theme]
    tiles = [rows for _, rows in art.TILES]
    width = stage.width * 16
    height = levels.ROWS * 16
    image = [bytearray(width * 3) for _ in range(height)]
    for row in range(levels.ROWS):
        for column in range(stage.width):
            tile = tiles[stage.map[row * stage.width + column]]
            for y in range(16):
                line = image[row * 16 + y]
                for x in range(16):
                    red, green, blue = colors[art.INK[tile[y][x]]]
                    offset = (column * 16 + x) * 3
                    line[offset:offset + 3] = bytes((red, green, blue))
    marks = {levels.NUTLET: "NUT_1", levels.BEETLE: "BEETLE_1", levels.LIFT_X: "LIFT", levels.LIFT_Y: "LIFT"}
    for kind, column, row in stage.objects + [(0, 3, 10)]:
        cell = sprites_by_name[marks.get(kind, "PIP_STAND")]
        for y in range(16):
            for x in range(16):
                index = art.INK[cell[y][x]]
                if index == 0:
                    continue
                red, green, blue = colors[index]
                py, px = row * 16 + y, column * 16 + x
                if 0 <= py < height and 0 <= px < width:
                    image[py][px * 3:px * 3 + 3] = bytes((red, green, blue))
    return width, height, image


def main():
    check_ids()
    os.makedirs(OUT, exist_ok=True)
    gfx = build_gfx()
    with open(os.path.join(OUT, "gfx.bin"), "wb") as output:
        output.write(gfx)
    print("gfx.bin %d bytes: %d tiles, %d sprites" % (len(gfx), len(art.TILES), len(art.SPRITES)))
    stages = [make() for make in levels.STAGES]
    for number, stage in enumerate(stages, 1):
        data = build_stage(stage)
        with open(os.path.join(OUT, "stage%d.bin" % number), "wb") as output:
            output.write(data)
        print("stage%d.bin %d bytes: %s, %d columns, %d objects" % (
            number, len(data), stage.name, stage.width, len(stage.objects)))
    print("game.mpy %d bytes" % compile_game())

    if "--preview" in sys.argv:
        directory = sys.argv[sys.argv.index("--preview") + 1]
        os.makedirs(directory, exist_ok=True)
        write_png(os.path.join(directory, "tiles.png"), *render_cells([r for _, r in art.TILES], art.BASE))
        write_png(os.path.join(directory, "sprites.png"), *render_cells([r for _, r in art.SPRITES], art.BASE))
        sprites_by_name = dict(art.SPRITES)
        for number, stage in enumerate(stages, 1):
            write_png(os.path.join(directory, "stage%d.png" % number), *render_stage(stage, sprites_by_name))
        print("previews in", directory)


if __name__ == "__main__":
    main()
