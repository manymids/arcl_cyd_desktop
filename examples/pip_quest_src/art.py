"""Pixel art for PIP QUEST: 16x16 cells, one character per pixel.

Every design here is original to this project. build.py packs the cells into
gfx.bin at 4 bits per pixel. In tiles '.' is the background colour; in sprites
it is transparent.
"""

# Character -> palette index.
INK = {
    ".": 0, "W": 1, "K": 2, "D": 3, "B": 4, "T": 5, "g": 6, "G": 7,
    "L": 8, "Y": 9, "O": 10, "R": 11, "C": 12, "S": 13, "A": 14, "N": 15,
}

# Base colours (index order above).
BASE = [
    (104, 152, 255),  # . sky
    (255, 255, 255),  # W
    (16, 16, 24),     # K
    (96, 48, 16),     # D dark brown
    (176, 96, 40),    # B brown
    (236, 176, 112),  # T tan
    (0, 104, 40),     # g dark green
    (40, 168, 64),    # G green
    (152, 224, 96),   # L light green
    (248, 200, 40),   # Y gold
    (240, 112, 32),   # O orange
    (208, 40, 40),    # R red
    (32, 176, 176),   # C teal
    (252, 208, 160),  # S skin
    (160, 160, 176),  # A grey
    (40, 48, 88),     # N navy
]


def variant(base, **changes):
    colors = list(base)
    for key, value in changes.items():
        colors[INK[key if len(key) == 1 else key[0]]] = value
    return colors


def recolor(base, pairs):
    colors = list(base)
    for index, value in pairs:
        colors[index] = value
    return colors


# Sprite palettes 1-3 are the same in every theme; palette 0 is the scenery.
CHARACTER = BASE
EMBER_FORM = recolor(BASE, [(12, (216, 40, 32)), (15, (236, 236, 236)), (10, (248, 200, 40))])
COMET_FORM = recolor(BASE, [(12, (248, 200, 40)), (15, (200, 40, 40)), (13, (255, 255, 255))])

THEMES = [
    # 0 meadow
    BASE,
    # 1 hollow (cave)
    recolor(BASE, [(0, (0, 0, 0)), (3, (8, 32, 72)), (4, (40, 88, 168)), (5, (112, 160, 224)),
                   (14, (96, 96, 128))]),
    # 2 canopy
    recolor(BASE, [(0, (120, 188, 255)), (6, (0, 88, 72)), (7, (32, 152, 96))]),
]


def mirror(rows):
    return [row[::-1] for row in rows]


def fill(char):
    return [char * 16] * 16


# ---------------------------------------------------------------------------
# Tiles. The list position is the tile number used by levels.py and game.py.

GRASS = [
    "LLLLLLLLLLLLLLLL",
    "GLLGLLLGLLLGLLGL",
    "GGGGGGGGGGGGGGGG",
    "gGGgGGGGgGGgGGGg",
    "BgBBBgBBBBgBBBgB",
    "BBBBBBBBBBBBBBBB",
    "BBTBBBBBBBDBBBBB",
    "BBBBBBBDBBBBBBTB",
    "BBBBBBBBBBBBBBBB",
    "BDBBBTBBBBBBBBBB",
    "BBBBBBBBBBBTBBBB",
    "BBBBBBBBDBBBBBBB",
    "BTBBBBBBBBBBBDBB",
    "BBBBBDBBBBBBBBBB",
    "BBBBBBBBBBTBBBBB",
    "BBBBBBBBBBBBBBBB",
]

DIRT = [
    "BBBBBBBBBBBBBBBB",
    "BBTBBBBBBBDBBBBB",
    "BBBBBBBDBBBBBBTB",
    "BBBBBBBBBBBBBBBB",
    "BDBBBTBBBBBBBBBB",
    "BBBBBBBBBBBTBBBB",
    "BBBBBBBBDBBBBBBB",
    "BTBBBBBBBBBBBDBB",
    "BBBBBDBBBBBBBBBB",
    "BBBBBBBBBBTBBBBB",
    "BBBBBBBBBBBBBBBB",
    "BBBDBBBBBBBBBTBB",
    "BBBBBBBTBBBBBBBB",
    "BTBBBBBBBBBDBBBB",
    "BBBBBBBBBBBBBBBB",
    "BBBBBBDBBBBBBBBB",
]

BRICK = [
    "DDDDDDDDDDDDDDDD",
    "TTTTTTTDTTTTTTTD",
    "TBBBBBBDTBBBBBBD",
    "TBBBBBBDTBBBBBBD",
    "TBBBBBBDTBBBBBBD",
    "TBBBBBBDTBBBBBBD",
    "TBBBBBBDTBBBBBBD",
    "BBBBBBBDBBBBBBBD",
    "DDDDDDDDDDDDDDDD",
    "TTTDTTTTTTTDTTTT",
    "TBBDTBBBBBBDTBBB",
    "TBBDTBBBBBBDTBBB",
    "TBBDTBBBBBBDTBBB",
    "TBBDTBBBBBBDTBBB",
    "TBBDTBBBBBBDTBBB",
    "BBBDBBBBBBBDBBBB",
]

# A teal crystal set in a stone frame.
GEM_BLOCK = [
    "NNNNNNNNNNNNNNNN",
    "NAAAAAAAAAAAAAAN",
    "NAWWWWWWWWWWWNAN",
    "NAWCCCCCCCCCCNAN",
    "NAWCCCCWWCCCCNAN",
    "NAWCCCWCCWCCCNAN",
    "NAWCCWCCCCWCCNAN",
    "NAWCWCCCCCCWCNAN",
    "NAWCCWCCCCWCCNAN",
    "NAWCCCWCCWCCCNAN",
    "NAWCCCCWWCCCCNAN",
    "NAWCCCCCCCCCCNAN",
    "NAWCCCCCCCCCCNAN",
    "NANNNNNNNNNNNNAN",
    "NAAAAAAAAAAAAAAN",
    "NNNNNNNNNNNNNNNN",
]

USED_BLOCK = [
    ".DDDDDDDDDDDDDD.",
    "DBBBBBBBBBBBBBBD",
    "DBDBBBBBBBBBBDBD",
    "DBBBBBBBBBBBBBBD",
    "DBBBBBBBBBBBBBBD",
    "DBBBBBBBBBBBBBBD",
    "DBBBBBBBBBBBBBBD",
    "DBBBBBBBBBBBBBBD",
    "DBBBBBBBBBBBBBBD",
    "DBBBBBBBBBBBBBBD",
    "DBBBBBBBBBBBBBBD",
    "DBBBBBBBBBBBBBBD",
    "DBBBBBBBBBBBBBBD",
    "DBDBBBBBBBBBBDBD",
    "DBBBBBBBBBBBBBBD",
    "DDDDDDDDDDDDDDDD",
]

HARD = [
    "WWWWWWWWWWWWWWWN",
    "WAAAAAAAAAAAAAAN",
    "WAAAAAAAAAAAAAAN",
    "WAAAAAAAAAAAAAAN",
    "WAAANNNNNNNWAAAN",
    "WAAANAAAAAAWAAAN",
    "WAAANAAAAAAWAAAN",
    "WAAANAAAAAAWAAAN",
    "WAAANAAAAAAWAAAN",
    "WAAANAAAAAAWAAAN",
    "WAAANAAAAAAWAAAN",
    "WAAANWWWWWWWAAAN",
    "WAAAAAAAAAAAAAAN",
    "WAAAAAAAAAAAAAAN",
    "WAAAAAAAAAAAAAAN",
    "NNNNNNNNNNNNNNNN",
]

# A hollow log standing on end, two tiles wide.
LOG_RIM = [
    "DDDDDDDDDDDDDDDD",
    "DTTTTTTTTTTTTTTT",
    "DTBBBBBBBBBBBBBB",
    "DTBTTTTTTTTTTTTT",
    "DTBTBBBBBBBBBBBB",
    "DTBTBKKKKKKKKKKK",
    "DTBTBKKKKKKKKKKK",
    "DTBTBKKKKKKKKKKK",
    "DTBTBBBBBBBBBBBB",
    "DTBTTTTTTTTTTTTT",
    "DTBBBBBBBBBBBBBB",
    "DTTTTTTTTTTTTTTT",
    "DDDDDDDDDDDDDDDD",
    ".DBBTBBBBBTBBBBB",
    ".DBBTBBBBBTBBBBB",
    ".DBBTBBBBBTBBBBB",
]

LOG_BODY = [
    ".DBBTBBBBBTBBBBB",
    ".DBBTBBBBBTBBBBB",
    ".DBBTBBBDBTBBBBB",
    ".DBBTBBBDBTBBBBB",
    ".DBBTBBBBBTBBBBB",
    ".DBBTBBBBBTBBBBB",
    ".DBBTBBBBBTBBBDB",
    ".DBBTBBBBBTBBBDB",
    ".DBBTBBBBBTBBBBB",
    ".DBBTBBBBBTBBBBB",
    ".DBBTBDBBBTBBBBB",
    ".DBBTBDBBBTBBBBB",
    ".DBBTBBBBBTBBBBB",
    ".DBBTBBBBBTBBBBB",
    ".DBBTBBBBBTBBBBB",
    ".DBBTBBBBBTBBBBB",
]

GEM = [
    "................",
    "................",
    "................",
    "................",
    "......WWWW......",
    ".....WYYYYO.....",
    "....WYYWYYYO....",
    "...WYYWYYYYYO...",
    "..OOOOOOOOOOOO..",
    "...OYYYYYYYYO...",
    "....OYYYYYYO....",
    ".....OYYYYO.....",
    "......OYYO......",
    ".......OO.......",
    "................",
    "................",
]

CLOUD_LEFT = [
    "................",
    "................",
    "................",
    "...........WWW..",
    ".........WWWWWWW",
    "....WWW.WWWWWWWW",
    "...WWWWWWWWWWWWW",
    "..WWWWWWWWWWWWWW",
    ".WWWWWWWWWWWWWWW",
    ".WWWWWWWWWWWWWWW",
    ".AWWWWWWWWWWWWWW",
    "..AAWWWWWWWWWWWW",
    "....AAAAAAAAAAAA",
    "................",
    "................",
    "................",
]

CLOUD_MIDDLE = [
    "................",
    "................",
    ".....WWWWW......",
    "...WWWWWWWWW....",
    "WWWWWWWWWWWWWWWW",
    "WWWWWWWWWWWWWWWW",
    "WWWWWWWWWWWWWWWW",
    "WWWWWWWWWWWWWWWW",
    "WWWWWWWWWWWWWWWW",
    "WWWWWWWWWWWWWWWW",
    "WWWWWWWWWWWWWWWW",
    "AWWWWWWWWWWWWWWA",
    "AAAAAAAAAAAAAAAA",
    "................",
    "................",
    "................",
]

PINE = [
    ".......gg.......",
    "......gGGg......",
    ".....gGGGGg.....",
    ".....gGLGGg.....",
    "....gGGGGGGg....",
    "...gGGLGGGGGg...",
    "....gGGGGLGg....",
    "...gGGGGGGGGg...",
    "..gGGLGGGGGGGg..",
    ".gGGGGGGGLGGGGg.",
    "...gGGGGGGGGg...",
    "..gGGGLGGGGGGg..",
    ".gGGGGGGGGGLGGg.",
    "gGGGGGGGGGGGGGGg",
    ".gggggggggggggg.",
    ".......DD.......",
]

PINE_TRUNK = ["......DBBD......"] * 16

BUSH = [
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "......LL........",
    "..LL.LGGL..LL...",
    ".LGGLGGGGLLGGL..",
    "LGGGGGGLGGGGGGL.",
    "LGGLGGGGGGGLGGGL",
    "gGGGGGGGGGGGGGGg",
    "gGGGGLGGGGGGGGGg",
    "gGGGGGGGGGGLGGGg",
    "gggggggggggggggg",
]

POLE_TOP = [
    "................",
    "................",
    "................",
    "................",
    "......gGGg......",
    ".....gGLLGg.....",
    ".....gGLGGg.....",
    ".....gGGGGg.....",
    "......gGGg......",
    ".......WA.......",
    ".......WA.......",
    ".......WA.......",
    ".......WA.......",
    ".......WA.......",
    ".......WA.......",
    ".......WA.......",
]

POLE = [".......WA......."] * 16

WALL = ["DDDDDDDDDDDDDDDD"] + ["TTTBTTTTTTTBTTTT"] * 15

ROOF_LEFT = [
    "...............R",
    "..............RR",
    ".............RRR",
    "............RRRR",
    "...........RRRRR",
    "..........RRRDRR",
    ".........RRRRRRR",
    "........RRRRRRRR",
    ".......RRRRRRDRR",
    "......RRRRRRRRRR",
    ".....RRRRRRRRRRR",
    "....RRRRRRDRRRRR",
    "...RRRRRRRRRRRRR",
    "..RRRRRRRRRRRRRR",
    ".DDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDD",
]

ROOF_MIDDLE = [
    "RRRRRRRRRRRRRRRR",
    "RRRRRRRRRRRRRRRR",
    "RRRDRRRRRRRDRRRR",
    "RRRRRRRRRRRRRRRR",
    "RRRRRRRRRRRRRRRR",
    "RDRRRRRDRRRRRRRD",
    "RRRRRRRRRRRRRRRR",
    "RRRRRRRRRRRRRRRR",
    "RRRRDRRRRRRRDRRR",
    "RRRRRRRRRRRRRRRR",
    "RRRRRRRRRRRRRRRR",
    "RRDRRRRRRDRRRRRR",
    "RRRRRRRRRRRRRRRR",
    "RRRRRRRRRRRRRRRR",
    "DDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDD",
]

DOOR_TOP = [
    "TTTBTTTTTTTBTTTT",
    "TTTBTTTTTTTBTTTT",
    "TTTBDDDDDDDBTTTT",
    "TTTDKKKKKKKDTTTT",
] + ["TTDKKKKKKKKKDTTT"] * 12

DOOR = ["TTDKKKKKKKKKDTTT"] * 16

WINDOW = [
    "TTTBTTTTTTTBTTTT",
    "TTTBTTTTTTTBTTTT",
    "TTDDDDDDDDDDDTTT",
    "TTDYYYYDYYYYDTTT",
    "TTDYYYYDYYYYDTTT",
    "TTDYYYYDYYYYDTTT",
    "TTDYYYYDYYYYDTTT",
    "TTDDDDDDDDDDDTTT",
    "TTDYYYYDYYYYDTTT",
    "TTDYYYYDYYYYDTTT",
    "TTDYYYYDYYYYDTTT",
    "TTDYYYYDYYYYDTTT",
    "TTDDDDDDDDDDDTTT",
    "TTTBTTTTTTTBTTTT",
    "TTTBTTTTTTTBTTTT",
    "TTTBTTTTTTTBTTTT",
]

CANOPY_LEFT = [
    "...LLLLLLLLLLLLL",
    "..LGGGGGGGGGGGGG",
    ".LGGLGGGGGLGGGGG",
    "LGGGGGGGGGGGGGGG",
    "LGGGGGGLGGGGGGLG",
    "gGGGGGGGGGGGGGGG",
    "gGGLGGGGGGGGGGGG",
    ".gGGGGGGGGGGLGGG",
    "..gggggggggggggg",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
]

CANOPY_MIDDLE = [
    "LLLLLLLLLLLLLLLL",
    "GGGGGGGGGGGGGGGG",
    "GGGLGGGGGGLGGGGG",
    "GGGGGGGGGGGGGGGG",
    "GLGGGGGGGLGGGGGG",
    "GGGGGGGGGGGGGGLG",
    "GGGGGLGGGGGGGGGG",
    "GGGGGGGGGGGGGGGG",
    "gggggggggggggggg",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
]

TRUNK = ["....DBBTBBTBD..."] * 16

TILES = [
    ("EMPTY", fill(".")),
    ("GRASS", GRASS),
    ("BRICK", BRICK),
    ("GEM_BLOCK", GEM_BLOCK),
    ("USED", USED_BLOCK),
    ("HARD", HARD),
    ("LOG_RIM_L", LOG_RIM),
    ("LOG_RIM_R", mirror(LOG_RIM)),
    ("LOG_L", LOG_BODY),
    ("LOG_R", mirror(LOG_BODY)),
    ("GEM", GEM),
    ("DIRT", DIRT),
    ("CLOUD_L", CLOUD_LEFT),
    ("CLOUD_M", CLOUD_MIDDLE),
    ("CLOUD_R", mirror(CLOUD_LEFT)),
    ("PINE", PINE),
    ("PINE_TRUNK", PINE_TRUNK),
    ("BUSH", BUSH),
    ("POLE_TOP", POLE_TOP),
    ("POLE", POLE),
    ("WALL", WALL),
    ("ROOF_L", ROOF_LEFT),
    ("ROOF_M", ROOF_MIDDLE),
    ("ROOF_R", mirror(ROOF_LEFT)),
    ("DOOR_TOP", DOOR_TOP),
    ("DOOR", DOOR),
    ("WINDOW", WINDOW),
    ("HIDDEN_LIFE", fill(".")),
    ("GEM_BLOCK_POWER", GEM_BLOCK),
    ("BRICK_GEMS", BRICK),
    ("BRICK_POWER", BRICK),
    ("BRICK_COMET", BRICK),
    ("CANOPY_L", CANOPY_LEFT),
    ("CANOPY_M", CANOPY_MIDDLE),
    ("CANOPY_R", mirror(CANOPY_LEFT)),
    ("TRUNK", TRUNK),
    # Stands in for a block while its sprite bounces; solid, drawn as nothing.
    ("BUMPING", fill(".")),
]

# ---------------------------------------------------------------------------
# Sprites. Pip faces right; the game flips cells for left.

PIP_HEAD_SMALL = [
    "................",
    ".....C....C.....",
    ".....CC..CC.....",
    "....CCCCCCCC....",
    "...CCCCCCCCCC...",
    "...CCCSSSSSSC...",
    "...CCSSKSSKSS...",
    "...CCSSKSSKSS...",
    "...CCSSSSSSSS...",
    "....CCSSSSSS....",
]


def pip_small(body):
    return PIP_HEAD_SMALL + body


PIP_STAND = pip_small([
    "....OOOOOOOOO...",
    "...SNNOONNNNS...",
    "...SNNNNNNNNS...",
    "....NNNNNNNN....",
    "....DDD..DDD....",
    "...DDDD..DDDD...",
])

PIP_WALK1 = pip_small([
    "....OOOOOOOOO...",
    "..SNNNOONNNNNS..",
    "....NNNNNNNN....",
    "...NNNNNNNNNN...",
    "..DDD.....DDD...",
    ".DDD.......DDD..",
])

PIP_WALK2 = pip_small([
    "....OOOOOOOOO...",
    "...NNNOONNNNS...",
    "...SNNNNNNNN....",
    "....NNNNNNNN....",
    ".....DDDDDD.....",
    ".....DDD.DDD....",
])

PIP_WALK3 = pip_small([
    "....OOOOOOOOO...",
    "...SNNOONNNNNS..",
    "....NNNNNNNN....",
    "....NNNNNNNNN...",
    "...DDD...DDD....",
    "..DDDD....DDD...",
])

PIP_SKID = [
    "................",
    "......C....C....",
    "......CC..CC....",
    ".....CCCCCCCC...",
    "....CCCCCCCCCC..",
    "....CSSSSSSCCC..",
    "....SSKSSKSSCC..",
    "....SSKSSKSSCC..",
    "....SSSSSSSSCC..",
    ".....SSSSSSCC...",
    "....OOOOOOOOO...",
    "..SNNNNNOONNS...",
    "....NNNNNNNN....",
    "....NNNNNNNNN...",
    "..DDDD...DDD....",
    ".DDDD.....DD....",
]

PIP_JUMP = pip_small([
    "..S.OOOOOOOOO...",
    "..SNNNOONNNN....",
    "...NNNNNNNNN....",
    "...NNNNNNNNS....",
    "..DDD...DDD.....",
    "..DD.....DD.....",
])

PIP_HURT = [
    "................",
    "...C........C...",
    "...CC......CC...",
    "...CCCCCCCCCC...",
    "..CCCCCCCCCCCC..",
    "..CCSSSSSSSSCC..",
    "..CSSKSSSSKSSC..",
    "..CSSSKSSKSSSC..",
    "..CSSSSSSSSSSC..",
    "S..CSSSKKSSSC..S",
    "SS.OOOOOOOOOO.SS",
    ".SSNNNNNNNNNNSS.",
    "...NNNNNNNNNN...",
    "...NNNNNNNNNN...",
    "...DDD....DDD...",
    "..DDDD....DDDD..",
]

PIP_BIG_HEAD = [
    "................",
    "................",
    ".....C.....C....",
    ".....CC...CC....",
    ".....CCC.CCC....",
    "....CCCCCCCCC...",
    "...CCCCCCCCCCC..",
    "...CCCCCCCCCCC..",
    "..CCCCSSSSSSSC..",
    "..CCCSSSSSSSSS..",
    "..CCSSSKSSSKSS..",
    "..CCSSSKSSSKSS..",
    "..CCSSSKSSSKSS..",
    "..CCSSSSSSSSSS..",
    "...CCSSSSSSSSS..",
    "....CSSSSRRSS...",
]

PIP_BIG_TORSO = [
    "...OOOOOOOOOOO..",
    "..OOOOOOOOOOOO..",
    "..OO.NNNNNNNN...",
    ".OO.NNNNNNNNNN..",
    ".O..NNNNNNNNNN..",
    "...SNNNNNNNNNS..",
    "...SNNNNNNNNNS..",
    "...SSNNYYNNNSS..",
    "....NNNYYNNNN...",
    "....NNNNNNNNN...",
    "....NNNNNNNNN...",
]


def pip_big(legs):
    return PIP_BIG_TORSO + legs


PIP_BIG_STAND = pip_big([
    "....NNNN.NNNN...",
    "....NNN...NNN...",
    "....DDD...DDD...",
    "...DDDD...DDDD..",
    "..DDDDD...DDDDD.",
])

PIP_BIG_WALK1 = pip_big([
    "...NNNN...NNNN..",
    "..NNNN.....NNN..",
    "..DDD.......DDD.",
    ".DDDD.......DDDD",
    ".DDD.........DDD",
])

PIP_BIG_WALK2 = pip_big([
    ".....NNNNNN.....",
    ".....NNNNNN.....",
    ".....DDDDDD.....",
    "....DDDDDDD.....",
    "....DDDDDDDD....",
])

PIP_BIG_WALK3 = pip_big([
    "....NNNN.NNNN...",
    "...NNN.....NNN..",
    "...DDD.....DDD..",
    "..DDDD.....DDDD.",
    "..DDD.......DDD.",
])

PIP_BIG_JUMP = [
    "...OOOOOOOOOOO..",
    "..OOOOOOOOOOOO.S",
    "..OO.NNNNNNNNNS.",
    ".OO.NNNNNNNNNN..",
    ".O..NNNNNNNNNN..",
    "..SNNNNNNNNNN...",
    ".S..NNNNNNNNN...",
    "....NNNYYNNNN...",
    "....NNNYYNNNN...",
    "....NNNNNNNNN...",
    "....NNNNNNNNN...",
    "...NNNN..NNNN...",
    "..NNNN....NNN...",
    "..DDD......DDD..",
    ".DDD.......DDD..",
    ".DD........DD...",
]

NUTLET_TOP = [
    "................",
    ".......DD.......",
    "......DDDD......",
    "....DDBBBBDD....",
    "...DBTBTBTBBD...",
    "..DBBBBBBBBBBD..",
    "..DDDDDDDDDDDD..",
    "..DTTTTTTTTTTD..",
    ".DTTKKTTTTKKTTD.",
    ".DTTTWKTTKWTTTD.",
    ".DTTTTTTTTTTTTD.",
    "..DTTTTKKTTTTD..",
    "...DTTTTTTTTD...",
    "....DDDDDDDD....",
]

NUTLET_1 = NUTLET_TOP + ["...KKK....KKK...", "..KKKK....KKKK.."]
NUTLET_2 = NUTLET_TOP + ["....KKK..KKK....", "....KKKK.KKKK..."]
NUTLET_FLAT = [
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "....DDDDDDDD....",
    "..DBTBTBTBTBBD..",
    "..DDDDDDDDDDDD..",
    ".DTTKKTTTTKKTTD.",
    ".DDDDDDDDDDDDDD.",
    "..KKK......KKK..",
]

BEETLE_SHELL_TOP = [
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "......gggggg....",
    "....ggGGLLGGgg..",
    "...gGGLLGGGGGGg.",
    "..gGGGGGGGLLGGGg",
]

BEETLE_1 = BEETLE_SHELL_TOP + [
    ".KKgGGGGGGGGGGGg",
    "KWKKgGGLLGGGGGGg",
    "KKKKggggggggggg.",
    ".KKKTTTTTTTTTTT.",
    "...K..K...K..K..",
    "..K..K...K..K...",
]
BEETLE_2 = BEETLE_SHELL_TOP + [
    ".KKgGGGGGGGGGGGg",
    "KWKKgGGLLGGGGGGg",
    "KKKKggggggggggg.",
    ".KKKTTTTTTTTTTT.",
    "..K..K...K..K...",
    "...K..K...K..K..",
]
BEETLE_SHELL = [
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    ".....gggggg.....",
    "...ggGGLLGGgg...",
    "..gGGLLGGGGGGg..",
    ".gGGGGGGGLLGGGg.",
    ".gGGLLGGGGGGGGg.",
    "..gggggggggggg..",
    "...TTTTTTTTTT...",
    "................",
]

SPROUT = [
    "................",
    "................",
    "..LLL......LLL..",
    ".LGGGL....LGGGL.",
    ".LGGGGL..LGGGGL.",
    "..LGGGGLLGGGGL..",
    "...LGGGGGGGGL...",
    "....LLLGGLLL....",
    ".......gg.......",
    ".......gg.......",
    ".....YYYYYY.....",
    "....YWWYYYYY....",
    "....YWYYYYYO....",
    "....YYYYYYYO....",
    ".....YYYYYO.....",
    "......OOOO......",
]

# A little lantern with a flame inside.
EMBER_FLOWER = [
    "................",
    ".......DD.......",
    "......D..D......",
    ".....DDDDDD.....",
    "....DBBBBBBD....",
    "....DWYYYYWD....",
    "....DWYOOYWD....",
    "....DWOROOWD....",
    "....DWORROWD....",
    "....DWOOROWD....",
    "....DWYOOYWD....",
    "....DWYYYYWD....",
    "....DBBBBBBD....",
    ".....DDDDDD.....",
    "......DDDD......",
    "................",
]

COMET = [
    "................",
    ".......W........",
    ".......W........",
    "......YWY.......",
    ".....YYWYY......",
    "....YYWWWYY.....",
    "...YYWWWWWYY....",
    "WWWWWWWWWWWWWWW.",
    "...YYWWWWWYY....",
    "....YYWWWYY.....",
    ".....YYWYY......",
    "......YWY.......",
    ".......W........",
    ".......W........",
    "................",
    "................",
]

HEART_BERRY = [
    "................",
    ".......gG.......",
    "........g.......",
    "..RRRRR.gRRRRR..",
    ".RRWWRRRRRRRRRR.",
    ".RWWRRRRRRRRRRR.",
    ".RWRRRRRRRRRRRR.",
    ".RRRRRRRRRRRRRR.",
    "..RRRRRRRRRRRR..",
    "...RRRRRRRRRR...",
    "....RRRRRRRR....",
    ".....RRRRRR.....",
    "......RRRR......",
    ".......RR.......",
    "................",
    "................",
]

GEM_NARROW = [
    "................",
    "................",
    "................",
    "................",
    ".......WW.......",
    "......WYYO......",
    "......WYYO......",
    "......WYYO......",
    "......OOOO......",
    "......OYYO......",
    "......OYYO......",
    ".......YO.......",
    ".......OO.......",
    "................",
    "................",
    "................",
]

GEM_EDGE = ["................"] * 4 + [".......WO......."] * 9 + ["................"] * 3

EMBER = [
    "................",
    "................",
    "................",
    "................",
    "......RR........",
    ".....ROOR.......",
    "....ROYYOR......",
    "....OYWWYO......",
    "....OYWWYO......",
    "....ROYYOR......",
    ".....ROOR.......",
    "......RR........",
    "................",
    "................",
    "................",
    "................",
]

PUFF = [
    "................",
    "................",
    "................",
    "......W..W......",
    "...W..Y..Y..W...",
    "....Y.O..O.Y....",
    "................",
    "..WYO......OYW..",
    "..WYO......OYW..",
    "................",
    "....Y.O..O.Y....",
    "...W..Y..Y..W...",
    "......W..W......",
    "................",
    "................",
    "................",
]

DEBRIS = [
    "................",
    "................",
    "................",
    "................",
    ".....TTTT.......",
    "....TBBBBD......",
    "....TBBBBD......",
    "....TBBDBD......",
    "....TBBBBD......",
    ".....DDDD.......",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
]

BANNER = [
    "................",
    "................",
    "....WWWWWWWWWWWW",
    "......WWWWGGWWWW",
    "........WGGGGWWW",
    "..........WGGWWW",
    "............WWWW",
    "..............WW",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
]

LIFT = [
    "WWWWWWWWWWWWWWWW",
    "WAAAAAAAAAAAAAAN",
    "WANAAAAAAAAAANAN",
    "WAAAAAAAAAAAAAAN",
    "NNNNNNNNNNNNNNNN",
] + ["................"] * 11

SPRITES = [
    ("PIP_STAND", PIP_STAND),
    ("PIP_WALK1", PIP_WALK1),
    ("PIP_WALK2", PIP_WALK2),
    ("PIP_WALK3", PIP_WALK3),
    ("PIP_SKID", PIP_SKID),
    ("PIP_JUMP", PIP_JUMP),
    ("PIP_HURT", PIP_HURT),
    ("BIG_HEAD", PIP_BIG_HEAD),
    ("BIG_STAND", PIP_BIG_STAND),
    ("BIG_WALK1", PIP_BIG_WALK1),
    ("BIG_WALK2", PIP_BIG_WALK2),
    ("BIG_WALK3", PIP_BIG_WALK3),
    ("BIG_JUMP", PIP_BIG_JUMP),
    ("NUT_1", NUTLET_1),
    ("NUT_2", NUTLET_2),
    ("NUT_FLAT", NUTLET_FLAT),
    ("BEETLE_1", BEETLE_1),
    ("BEETLE_2", BEETLE_2),
    ("SHELL", BEETLE_SHELL),
    ("SPROUT", SPROUT),
    ("EMBER_FLOWER", EMBER_FLOWER),
    ("COMET", COMET),
    ("HEART", HEART_BERRY),
    ("GEM", GEM),
    ("GEM_NARROW", GEM_NARROW),
    ("GEM_EDGE", GEM_EDGE),
    ("EMBER", EMBER),
    ("PUFF", PUFF),
    ("DEBRIS", DEBRIS),
    ("BANNER", BANNER),
    ("LIFT", LIFT),
    ("BUMP_BRICK", BRICK),
    ("BUMP_GEM_BLOCK", GEM_BLOCK),
    ("BUMP_USED", USED_BLOCK),
]
