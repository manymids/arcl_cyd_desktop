"""Pixel art for EMBER VALE, an RPG-style demo. All designs are original.

Tiles and sprites are 16x16 cells, one character per pixel; the two monsters
are drawn procedurally at 48x48 and cut into 3x3 cells.
"""

INK = {
    ".": 0, "W": 1, "d": 2, "a": 3, "g": 4, "G": 5, "L": 6, "B": 7,
    "T": 8, "n": 9, "b": 10, "c": 11, "R": 12, "Y": 13, "S": 14, "P": 15,
}

BASE = [
    (0, 0, 0),        # . black: window fill, outside the world
    (255, 255, 255),  # W
    (72, 72, 88),     # d dark grey
    (168, 168, 184),  # a light grey
    (24, 96, 40),     # g dark green
    (56, 152, 56),    # G green
    (128, 208, 88),   # L light green
    (120, 72, 32),    # B brown
    (216, 176, 112),  # T tan
    (24, 40, 120),    # n dark blue
    (48, 96, 216),    # b blue
    (128, 192, 255),  # c light blue
    (200, 48, 40),    # R red
    (248, 208, 56),   # Y gold
    (248, 200, 160),  # S skin
    (136, 64, 168),   # P purple
]

VILLAGER_COLORS = [(i, c) for i, c in ((12, (136, 64, 168)), (10, (56, 152, 56)))]
MONSTER_VARIANT = [(4, (120, 40, 24)), (5, (200, 96, 40)), (6, (248, 176, 72)),
                   (15, (48, 96, 216)), (12, (128, 192, 255))]


def recolor(colors, pairs):
    colors = list(colors)
    for index, value in pairs:
        colors[index] = value
    return colors


# Palette 0: scenery and windows. Palette 1: hero. 2: villagers. 3: monster variants.
PALETTES = [BASE, BASE, recolor(BASE, VILLAGER_COLORS), recolor(BASE, MONSTER_VARIANT)]
# The water shimmer swaps blue and light blue in palette 0.
PALETTES_SHIMMER = [recolor(BASE, [(10, BASE[11]), (11, BASE[10])])] + PALETTES[1:]


def mirror(rows):
    return [row[::-1] for row in rows]


def flip(rows):
    return list(reversed(rows))


def rotate(rows):
    """Quarter turn clockwise."""
    return ["".join(rows[15 - y][x] for y in range(16)) for x in range(16)]


def speckle(base, marks):
    rows = [list(base * 16) for _ in range(16)]
    for x, y, ink in marks:
        rows[y][x] = ink
    return ["".join(row) for row in rows]


# ---------------------------------------------------------------------------
# Tiles

GRASS = speckle("G", [(4, 1, "L"), (11, 2, "g"), (2, 3, "g"), (8, 4, "L"), (14, 5, "L"), (1, 6, "L"),
                      (6, 6, "g"), (12, 8, "g"), (5, 9, "L"), (1, 10, "g"), (10, 11, "L"), (7, 12, "g"),
                      (3, 13, "L"), (14, 13, "g"), (9, 15, "g")])

FLOWERS = speckle("G", [(3, 2, "R"), (2, 3, "Y"), (4, 3, "Y"), (3, 4, "R"), (11, 5, "W"), (10, 6, "Y"),
                        (12, 6, "Y"), (11, 7, "W"), (6, 10, "R"), (5, 11, "Y"), (7, 11, "Y"), (6, 12, "R"),
                        (13, 12, "L"), (1, 8, "g"), (14, 1, "L")])

TREE = [
    "GGGGGggggggGGGGG",
    "GGGgggLLggggggGG",
    "GGggLLggggggggGG",
    "GgggLggggggLgggG",
    "GgLgggggggggggdG",
    "ggLggggLggggggdg",
    "gggggggggggggddg",
    "ggggggggggLgggdg",
    "gggLgggggggggddg",
    "Gggggggggggggdgd",
    "Ggggggggggggdddd",
    "GGddggggggddddGG",
    "GGGGddBBBdddGGGG",
    "GGGGGGBBBGGGGGGG",
    "GGGGGdBBBBdGGGGG",
    "GGGGGGGGGGGGGGGG",
]

MOUNTAIN = [
    "GGGGGGGWWGGGGGGG",
    "GGGGGGWWWaGGGGGG",
    "GGGGGWWWaaBGGGGG",
    "GGGGWWaTaBBGGGGG",
    "GGGGWaTTTBBBGGGG",
    "GGGTaTTTTBBBBGGG",
    "GGGTTTTTTBBBBGGG",
    "GGTTTTTBTTBBBBGG",
    "GGTTTTTTTBBBBBGG",
    "GTTTBTTTTTBBBBBG",
    "GTTTTTTTTBBBBBBG",
    "TTTTTTTTTTBBBBBB",
    "TTTTTBTTTTTBBBBB",
    "TTTTTTTTTTBBBBBB",
    "gTTTTTTTTBBBBBBg",
    "GGGGGGGGGGGGGGGG",
]

CAVE = [
    "GGGGGGGWWGGGGGGG",
    "GGGGGGWWWaGGGGGG",
    "GGGGGWWWaaBGGGGG",
    "GGGGWWaTaBBGGGGG",
    "GGGGWaTTTBBBGGGG",
    "GGGTaTTTTBBBBGGG",
    "GGGTTTTTTBBBBGGG",
    "GGTTTTBBBBBBBBGG",
    "GGTTTB......BBGG",
    "GTTTB........BBG",
    "GTTTB........BBG",
    "TTTB..........BB",
    "TTTB..........BB",
    "TTTB..........BB",
    "gTTB..........Bg",
    "GGGGGGGGGGGGGGGG",
]

WATER = [
    "bbbbbbbbbbbbbbbb",
    "bbbccbbbbbbbbbbb",
    "bbcbbcbbbbbbbbbb",
    "bbbbbbbbbbbccbbb",
    "bbbbbbbbbbcbbcbb",
    "bbbbbbbbbbbbbbbb",
    "bbbbbbbbbbbbbbbb",
    "bbbbbbccbbbbbbbb",
    "bbbbbcbbcbbbbbbb",
    "bbbbbbbbbbbbbbbb",
    "bcbbbbbbbbbbbbbb",
    "cbcbbbbbbbbbbccb",
    "bbbbbbbbbbbbcbbc",
    "bbbbbbbbbbbbbbbb",
    "bbbbbbbbbbbbbbbb",
    "bbbbbbbbbbbbbbbb",
]

BRIDGE = [
    "dBTBBBTBBBTBBBTd",
    "dBTBBBTBBBTBBBTd",
    "dBTBBBTBBBTBBBTd",
    "dBTBBBTBBBTBBBTd",
    "dBBBBBBBBBBBBBBd",
    "dBTBBBTBBBTBBBTd",
    "dBTBBBTBBBTBBBTd",
    "dBTBBBTBBBTBBBTd",
    "dBTBBBTBBBTBBBTd",
    "dBBBBBBBBBBBBBBd",
    "dBTBBBTBBBTBBBTd",
    "dBTBBBTBBBTBBBTd",
    "dBTBBBTBBBTBBBTd",
    "dBTBBBTBBBTBBBTd",
    "dBBBBBBBBBBBBBBd",
    "dBTBBBTBBBTBBBTd",
]

PATH = speckle("T", [(3, 2, "B"), (12, 3, "B"), (7, 6, "a"), (1, 9, "B"), (10, 10, "B"), (5, 13, "a"),
                     (14, 14, "B"), (8, 0, "a")])

WALL = [
    "dddddddddddddddd",
    "aaaaaaadaaaaaaad",
    "aWaaaaadaWaaaaad",
    "aaaaaaadaaaaaaad",
    "aaaaaaadaaaaaaad",
    "aaaaaaadaaaaaaad",
    "aaaaaaadaaaaaaad",
    "dddddddddddddddd",
    "aaadaaaaaaadaaaa",
    "aWadaWaaaaadaWaa",
    "aaadaaaaaaadaaaa",
    "aaadaaaaaaadaaaa",
    "aaadaaaaaaadaaaa",
    "aaadaaaaaaadaaaa",
    "aaadaaaaaaadaaaa",
    "dddddddddddddddd",
]

FLOOR = [
    "TTTTTTTBTTTTTTTB",
    "TTTTTTTBTTTTTTTB",
    "TTTTTTTBTTTTTTTB",
    "BBBBBBBBBBBBBBBB",
    "TTTBTTTTTTTBTTTT",
    "TTTBTTTTTTTBTTTT",
    "TTTBTTTTTTTBTTTT",
    "BBBBBBBBBBBBBBBB",
    "TTTTTTTBTTTTTTTB",
    "TTTTTTTBTTTTTTTB",
    "TTTTTTTBTTTTTTTB",
    "BBBBBBBBBBBBBBBB",
    "TTTBTTTTTTTBTTTT",
    "TTTBTTTTTTTBTTTT",
    "TTTBTTTTTTTBTTTT",
    "BBBBBBBBBBBBBBBB",
]

HOUSE = [
    "BBBBBBBBBBBBBBBB",
    "TTTTBTTTTTTBTTTT",
    "TTTTBTTTTTTBTTTT",
    "TTTTBTTTTTTBTTTT",
    "TTTTBTTTTTTBTTTT",
    "BBBBBBBBBBBBBBBB",
    "TTTTBTTTTTTBTTTT",
    "TTTTBTTTTTTBTTTT",
    "TTTTBTTTTTTBTTTT",
    "TTTTBTTTTTTBTTTT",
    "BBBBBBBBBBBBBBBB",
    "TTTTBTTTTTTBTTTT",
    "TTTTBTTTTTTBTTTT",
    "TTTTBTTTTTTBTTTT",
    "TTTTBTTTTTTBTTTT",
    "BBBBBBBBBBBBBBBB",
]

ROOF = [
    "RRRRRRRRRRRRRRRR",
    "RRRRRRRRRRRRRRRR",
    "BRRRBRRRBRRRBRRR",
    "RRRRRRRRRRRRRRRR",
    "RRRRRRRRRRRRRRRR",
    "RRBRRRBRRRBRRRBR",
    "RRRRRRRRRRRRRRRR",
    "RRRRRRRRRRRRRRRR",
    "BRRRBRRRBRRRBRRR",
    "RRRRRRRRRRRRRRRR",
    "RRRRRRRRRRRRRRRR",
    "RRBRRRBRRRBRRRBR",
    "RRRRRRRRRRRRRRRR",
    "RRRRRRRRRRRRRRRR",
    "BBBBBBBBBBBBBBBB",
    "dddddddddddddddd",
]

DOOR = [
    "BBBBBBBBBBBBBBBB",
    "TTTBBBBBBBBBBTTT",
    "TTBddddddddddBTT",
    "TTBdBBBBBBBBdBTT",
    "TTBdBTBBBBTBdBTT",
    "TTBdBTBBBBTBdBTT",
    "TTBdBTBBBBTBdBTT",
    "TTBdBTBBBBTBdBTT",
    "TTBdBBBBBBBYdBTT",
    "TTBdBTBBBBTBdBTT",
    "TTBdBTBBBBTBdBTT",
    "TTBdBTBBBBTBdBTT",
    "TTBdBTBBBBTBdBTT",
    "TTBdBBBBBBBBdBTT",
    "TTBddddddddddBTT",
    "BBBBBBBBBBBBBBBB",
]

CHEST = [
    "TTTTTTTBTTTTTTTB",
    "TTTTTTTBTTTTTTTB",
    "TTTTTTTBTTTTTTTB",
    "BBddddddddddddBB",
    "TdBBBBBBBBBBBBdT",
    "TdBYYYYYYYYYYBdT",
    "TdBBBBBYYBBBBBdT",
    "BdddddddYYddddBB",
    "TdBBBBBYYBBBBBdT",
    "TdBBBBBBBBBBBBdT",
    "TdBYBBBBBBBBYBdT",
    "BdBBBBBBBBBBBBdB",
    "TdddddddddddddTT",
    "TTTBTTTTTTTBTTTT",
    "TTTBTTTTTTTBTTTT",
    "BBBBBBBBBBBBBBBB",
]

CHEST_OPEN = [
    "TTTTTTTBTTTTTTTB",
    "TTdddddddddddddB",
    "TdBBBBBBBBBBBBdB",
    "BdBBBBBBBBBBBBdB",
    "Tdddddddddddddd.",
    "Td............dT",
    "Td............dT",
    "BdddddddddddddBB",
    "TdBBBBBBBBBBBBdT",
    "TdBBBBBBBBBBBBdT",
    "TdBYBBBBBBBBYBdT",
    "BdBBBBBBBBBBBBdB",
    "TdddddddddddddTT",
    "TTTBTTTTTTTBTTTT",
    "TTTBTTTTTTTBTTTT",
    "BBBBBBBBBBBBBBBB",
]

WELL = [
    "TTTTTTTBTTTTTTTB",
    "TTTTddddddddTTTB",
    "TTddaaaaaaaaddTB",
    "TdaaddddddddaadB",
    "BdadnnnnnnnndadB",
    "TdadnbbbbbbndadT",
    "TdadnbcbbbbndadT",
    "TdadnbbbbcbndadT",
    "BdadnnnnnnnndadB",
    "TdaaddddddddaadT",
    "TTddaaaaaaaaddTT",
    "TTTTddddddddTTTT",
    "TTTBTTTTTTTBTTTT",
    "BBBBBBBBBBBBBBBB",
    "TTTTTTTBTTTTTTTB",
    "TTTTTTTBTTTTTTTB",
]

TILES = [
    ("VOID", ["." * 16] * 16),
    ("GRASS", GRASS),
    ("FLOWERS", FLOWERS),
    ("TREE", TREE),
    ("MOUNTAIN", MOUNTAIN),
    ("CAVE", CAVE),
    ("WATER", WATER),
    ("BRIDGE", BRIDGE),
    ("PATH", PATH),
    ("WALL", WALL),
    ("FLOOR", FLOOR),
    ("HOUSE", HOUSE),
    ("ROOF", ROOF),
    ("DOOR", DOOR),
    ("CHEST", CHEST),
    ("CHEST_OPEN", CHEST_OPEN),
    ("WELL", WELL),
]

# ---------------------------------------------------------------------------
# Sprites

HERO_HEAD_DOWN = [
    "................",
    ".....BBBBBB.....",
    "....BBBBBBBB....",
    "...BRRRRRRRRB...",
    "...BSSSSSSSSB...",
    "...SSdSSSSdSS...",
    "...SSdSSSSdSS...",
    "....SSSSSSSS....",
]

HERO_DOWN_1 = HERO_HEAD_DOWN + [
    "....bbbYYbbb....",
    "...bbbbYYbbbb...",
    "..SbbbbbbbbbbS..",
    "..SbbbbbbbbbbS..",
    "...BBBBBBBBBB...",
    "....bbbb.bbbb...",
    "....BBB...BBB...",
    "...BBBB...BBBB..",
]

HERO_DOWN_2 = HERO_HEAD_DOWN + [
    "....bbbYYbbb....",
    "...bbbbYYbbbb...",
    "..SbbbbbbbbbbS..",
    "...bbbbbbbbbbS..",
    "..SBBBBBBBBBB...",
    "....bbbb.bbbb...",
    ".....BB...BBB...",
    "....BBB...BBBB..",
]

HERO_HEAD_UP = [
    "................",
    ".....BBBBBB.....",
    "....BBBBBBBB....",
    "...BRRRRRRRRB...",
    "...BBBBBBBBBB...",
    "...SBBBBBBBBS...",
    "...SBBBRRBBBS...",
    "....BBBRRBBB....",
]

HERO_UP_1 = HERO_HEAD_UP + [
    "....bbbbbbbb....",
    "...bbbbbbbbbb...",
    "..SbbbabbabbbS..",
    "..SbbbaabbbbbS..",
    "...BBBBBBBBBB...",
    "....bbbb.bbbb...",
    "....BBB...BBB...",
    "...BBBB...BBBB..",
]

HERO_UP_2 = HERO_HEAD_UP + [
    "....bbbbbbbb....",
    "...bbbbbbbbbb...",
    "..SbbbabbabbbS..",
    "..SbbbaabbbbbS..",
    "...BBBBBBBBBB...",
    "....bbbb.bbbb...",
    "....BBB...BB....",
    "...BBBB...BBB...",
]

HERO_HEAD_SIDE = [
    "................",
    ".....BBBBBB.....",
    "....BBBBBBBBB...",
    "....RRRRRRRRRR..",
    "...RBBBBSSSSB...",
    "..RRBBBSSSdSS...",
    "..R.BBBSSSdSSS..",
    "....BBBSSSSSS...",
]

HERO_SIDE_1 = HERO_HEAD_SIDE + [
    ".....bbbYbbb....",
    "....bbbbYbbb....",
    "....bbbbSSbb....",
    "....abbbbbbb....",
    "....BBBBBBBB....",
    ".....bbbbbb.....",
    ".....BBB.BB.....",
    "....BBBB.BBB....",
]

HERO_SIDE_2 = HERO_HEAD_SIDE + [
    ".....bbbYbbb....",
    "....bbbbYbbb....",
    "....bbbSSbbb....",
    "....abbbbbbb....",
    "....BBBBBBBB....",
    "....bbb..bbb....",
    "...BBB....BBB...",
    "..BBBB....BBBB..",
]

VILLAGER_1 = [
    "................",
    ".....RRRRRR.....",
    "....RRRRRRRR....",
    "...RRBBBBBBRR...",
    "...RBSSSSSSBR...",
    "....SSdSSdSS....",
    "....SSdSSdSS....",
    ".....SSSSSS.....",
    "....bbbWWbbb....",
    "...bbbbWWbbbb...",
    "..SbbbbbbbbbbS..",
    "..SbbbbbbbbbbS..",
    "...bbbbbbbbbb...",
    "...bbbbbbbbbb...",
    "....BBB..BBB....",
    "...BBBB..BBBB...",
]

VILLAGER_2 = VILLAGER_1[:12] + [
    "...bbbbbbbbbbS..",
    "...bbbbbbbbbb...",
    "....BBB..BBB....",
    "...BBBB..BBBB...",
]

ELDER_1 = [
    "................",
    "......aaaa......",
    ".....aaaaaa.....",
    "....aSSSSSSa....",
    "....SSdSSdSS....",
    "....SSSSSSSS....",
    "....WWSSSSWW....",
    "....WWWWWWWW....",
    "...PPWWWWWWPP...",
    "..PPPPWWWWPPPP..",
    "..SPPPPWWPPPPS..",
    "..BPPPPPPPPPPB..",
    "..BPPPPPPPPPP...",
    "..BPPPPPPPPPP...",
    "..B.BBB..BBB....",
    "..B.............",
]

ELDER_2 = ELDER_1[:10] + [
    "..SPPPPWWPPPPS..",
    "...PPPPPPPPPPB..",
    "...PPPPPPPPPPB..",
    "...PPPPPPPPPPB..",
    "....BBB..BBBB...",
    "..............B.",
]

CURSOR = [
    "................",
    "................",
    "................",
    "....W...........",
    "....WW..........",
    "....WWW.........",
    "....WWWW........",
    "....WWWWW.......",
    "....WWWW........",
    "....WWW.........",
    "....WW..........",
    "....W...........",
    "................",
    "................",
    "................",
    "................",
]

ARROW_UP = [
    "................",
    "................",
    "................",
    ".......WW.......",
    "......WWWW......",
    ".....WWWWWW.....",
    "....WWWWWWWW....",
    "...WWWWWWWWWW...",
    "..WWWWWWWWWWWW..",
    ".....WWWWWW.....",
    ".....WWWWWW.....",
    ".....WWWWWW.....",
    ".....WWWWWW.....",
    "................",
    "................",
    "................",
]


# ---------------------------------------------------------------------------
# Monsters, drawn at 48x48.

def canvas():
    return [["."] * 48 for _ in range(48)]


def ellipse(grid, cx, cy, rx, ry, ink):
    for y in range(48):
        for x in range(48):
            if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0:
                grid[y][x] = ink


def outline(grid, ink):
    """Draw ink around every filled pixel that touches the background."""
    marks = []
    for y in range(48):
        for x in range(48):
            if grid[y][x] != ".":
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < 48 and 0 <= ny < 48 and grid[ny][nx] not in (".", ink):
                    marks.append((x, y))
                    break
    for x, y in marks:
        grid[y][x] = ink


def triangle(grid, points, ink):
    (x0, y0), (x1, y1), (x2, y2) = points
    def side(ax, ay, bx, by, px, py):
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax)
    for y in range(48):
        for x in range(48):
            d0 = side(x0, y0, x1, y1, x, y)
            d1 = side(x1, y1, x2, y2, x, y)
            d2 = side(x2, y2, x0, y0, x, y)
            if (d0 >= 0 and d1 >= 0 and d2 >= 0) or (d0 <= 0 and d1 <= 0 and d2 <= 0):
                grid[y][x] = ink


def mossback():
    g = canvas()
    for cx in (10, 38):
        ellipse(g, cx, 42, 6, 4, "B")
    ellipse(g, 24, 28, 21, 15, "g")
    ellipse(g, 20, 22, 12, 7, "G")
    for cx, cy in ((12, 18), (24, 14), (34, 20), (30, 30), (16, 30)):
        ellipse(g, cx, cy, 3, 2, "L")
    ellipse(g, 24, 37, 15, 8, "T")
    for cx in (17, 31):
        ellipse(g, cx, 33, 4, 4, "W")
        ellipse(g, cx + 1, 34, 1.6, 1.6, "d")
    for x in range(19, 30):
        g[40][x] = "d"
    g[39][19] = g[39][29] = "d"
    ellipse(g, 24, 10, 2, 3, "G")
    for cx, cy in ((21, 6), (27, 6), (24, 3)):
        ellipse(g, cx, cy, 2, 2, "R")
    ellipse(g, 24, 6, 1.5, 1.5, "Y")
    outline(g, "d")
    return g


def emberwing():
    g = canvas()
    triangle(g, ((22, 18), (1, 8), (6, 36)), "d")
    triangle(g, ((26, 18), (47, 8), (42, 36)), "d")
    for x0, x1 in ((3, 20), (45, 28)):
        for step in range(14):
            x = x0 + (x1 - x0) * step // 14
            y = 12 + step
            g[y][x] = "P"
    triangle(g, ((24, 30), (18, 46), (30, 46)), "R")
    triangle(g, ((24, 34), (21, 44), (27, 44)), "Y")
    ellipse(g, 24, 24, 10, 12, "P")
    ellipse(g, 24, 28, 6, 6, "R")
    triangle(g, ((24, 0), (16, 14), (32, 14)), "R")
    triangle(g, ((24, 5), (20, 14), (28, 14)), "Y")
    for cx in (20, 28):
        ellipse(g, cx, 21, 2.5, 2, "Y")
        g[21][cx] = "W"
    for x in (21, 27):
        g[29][x] = g[30][x] = "W"
    outline(g, "n")
    return g


def cells_of(grid):
    cells = []
    for cy in range(3):
        for cx in range(3):
            cells.append(["".join(grid[cy * 16 + y][cx * 16:cx * 16 + 16]) for y in range(16)])
    return cells


SPRITES = [
    ("HERO_DOWN_1", HERO_DOWN_1),
    ("HERO_DOWN_2", HERO_DOWN_2),
    ("HERO_UP_1", HERO_UP_1),
    ("HERO_UP_2", HERO_UP_2),
    ("HERO_SIDE_1", HERO_SIDE_1),
    ("HERO_SIDE_2", HERO_SIDE_2),
    ("VILLAGER_1", VILLAGER_1),
    ("VILLAGER_2", VILLAGER_2),
    ("ELDER_1", ELDER_1),
    ("ELDER_2", ELDER_2),
]
SPRITES += [("MOSSBACK_%d" % i, cell) for i, cell in enumerate(cells_of(mossback()))]
SPRITES += [("EMBERWING_%d" % i, cell) for i, cell in enumerate(cells_of(emberwing()))]
