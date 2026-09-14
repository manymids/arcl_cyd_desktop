"""Stage layouts for PIP QUEST, written with a few placement helpers.

A stage is 13 rows of 16x16 tiles: row 0 at the top, the ground on rows 11
and 12. Objects (enemies, lifts) are kept apart from the tile map and spawn
as the screen reaches their column.
"""

from art import TILES

T = {name: index for index, (name, _) in enumerate(TILES)}
ROWS = 13

# Object kinds, shared with game.py.
NUTLET = 1
BEETLE = 2
LIFT_X = 3   # moves right and back 6 tiles
LIFT_Y = 4   # moves down and back 6 tiles


class Stage:
    def __init__(self, name, width, theme, time):
        self.name = name
        self.width = width
        self.theme = theme
        self.time = time
        self.map = bytearray(width * ROWS)
        self.objects = []
        self.goal = 0
        self.checkpoint = 0

    def put(self, column, row, tile):
        if 0 <= column < self.width and 0 <= row < ROWS:
            self.map[row * self.width + column] = T[tile] if isinstance(tile, str) else tile

    def ground(self, first, end, top="GRASS", fill="DIRT"):
        for column in range(first, end):
            self.put(column, 11, top)
            self.put(column, 12, fill)

    def row(self, column, row, pattern):
        """b brick, ? gem block, P gem block with a power-up, G brick of gems,
        U brick with a power-up, C brick with a comet, H hard, * gem, L hidden life."""
        names = {"b": "BRICK", "?": "GEM_BLOCK", "P": "GEM_BLOCK_POWER", "G": "BRICK_GEMS",
                 "U": "BRICK_POWER", "C": "BRICK_COMET", "H": "HARD", "*": "GEM",
                 "L": "HIDDEN_LIFE", "u": "USED"}
        for offset, symbol in enumerate(pattern):
            if symbol != " ":
                self.put(column + offset, row, names[symbol])

    def log(self, column, height, base=10):
        top = base - height + 1
        self.put(column, top, "LOG_RIM_L")
        self.put(column + 1, top, "LOG_RIM_R")
        for row in range(top + 1, base + 1):
            self.put(column, row, "LOG_L")
            self.put(column + 1, row, "LOG_R")

    def column(self, column, height, tile="HARD", base=10):
        for row in range(base - height + 1, base + 1):
            self.put(column, row, tile)

    def stairs(self, column, heights, tile="HARD", base=10):
        for offset, height in enumerate(heights):
            self.column(column + offset, height, tile, base)

    def cloud(self, column, row, length=1):
        self.put(column, row, "CLOUD_L")
        for offset in range(length):
            self.put(column + 1 + offset, row, "CLOUD_M")
        self.put(column + 1 + length, row, "CLOUD_R")

    def pine(self, column, base=10):
        self.put(column, base - 1, "PINE")
        self.put(column, base, "PINE_TRUNK")

    def canopy(self, column, row, length):
        self.put(column, row, "CANOPY_L")
        for offset in range(1, length - 1):
            self.put(column + offset, row, "CANOPY_M")
        self.put(column + length - 1, row, "CANOPY_R")
        middle = column + length // 2
        for trunk_row in range(row + 1, ROWS):
            self.put(middle, trunk_row, "TRUNK")

    def enemy(self, kind, column, row=10):
        self.objects.append((kind, column, row))

    def finish(self, pole, cottage):
        self.goal = pole
        self.put(pole, 10, "HARD")
        self.put(pole, 1, "POLE_TOP")
        for row in range(2, 10):
            self.put(pole, row, "POLE")
        # A cottage five tiles wide with the door in the middle.
        for offset in range(5):
            self.put(cottage + offset, 7, "ROOF_M")
            self.put(cottage + offset, 8, "WALL")
            self.put(cottage + offset, 9, "WALL")
            self.put(cottage + offset, 10, "WALL")
        self.put(cottage - 1, 7, "ROOF_L")
        self.put(cottage + 5, 7, "ROOF_R")
        self.put(cottage + 1, 9, "WINDOW")
        self.put(cottage + 3, 9, "WINDOW")
        self.put(cottage + 2, 9, "DOOR_TOP")
        self.put(cottage + 2, 10, "DOOR")


def meadow():
    s = Stage("1-1", 200, theme=0, time=400)
    s.checkpoint = 96
    s.ground(0, 41)
    s.ground(45, 96)
    s.ground(100, 135)
    s.ground(138, 200)
    for column, row in ((5, 2), (23, 3), (41, 2), (61, 3), (78, 2), (97, 3), (116, 2),
                        (133, 3), (151, 2), (171, 3), (186, 2)):
        s.cloud(column, row, 1 + column % 2)
    for column in (9, 30, 51, 70, 89, 106, 121, 147, 168):
        s.pine(column)
    for column in (3, 18, 36, 58, 76, 102, 126, 142, 160):
        s.put(column, 10, "BUSH")

    s.row(8, 8, "*  *")
    s.row(9, 7, "**")
    s.row(14, 7, "b?b")
    s.enemy(NUTLET, 22)
    s.stairs(24, (1, 2, 3, 3, 2, 1))
    s.row(24, 5, "  **  ")
    s.row(32, 7, "bPb")
    s.row(32, 3, "***")
    s.log(38, 2)
    s.enemy(NUTLET, 34)

    s.enemy(BEETLE, 48)
    s.row(50, 7, "bbbGbbbb")
    s.row(55, 3, "bbbPbbb")
    s.enemy(NUTLET, 56, 2)
    s.enemy(NUTLET, 59, 2)
    s.log(65, 3)
    s.enemy(NUTLET, 68)
    s.enemy(NUTLET, 70)
    s.log(73, 2)
    s.row(79, 7, "L")
    s.row(84, 8, "HHHH")
    s.row(84, 6, "****")
    s.row(96, 7, " ** ")

    s.row(104, 7, "?  ?  ?")
    s.row(107, 3, "P")
    s.row(113, 7, "bCb")
    s.enemy(NUTLET, 117)
    s.enemy(NUTLET, 119)
    s.enemy(NUTLET, 121)
    s.enemy(BEETLE, 125)
    s.stairs(128, (1, 2, 3, 4, 4, 4, 4))
    s.row(129, 4, " ***")
    s.stairs(138, (4, 3, 2, 1))

    s.log(150, 4)
    s.enemy(NUTLET, 155)
    s.enemy(NUTLET, 157)
    s.row(159, 7, "bb?bG")
    s.enemy(BEETLE, 166)
    s.stairs(174, (1, 2, 2, 1))
    s.finish(184, 189)
    return s


def hollow():
    s = Stage("1-2", 184, theme=1, time=400)
    s.checkpoint = 90
    s.ground(0, 40, "DIRT")
    s.ground(44, 80, "DIRT")
    s.ground(84, 120, "DIRT")
    s.ground(126, 184, "DIRT")
    for column in range(6, 172):
        s.put(column, 0, "BRICK")
    for column in range(0, 6):
        s.column(column, 0)
    s.column(6, 10, "BRICK")

    s.row(11, 7, "P")
    s.row(15, 7, "bbbbb")
    s.row(15, 10, "*****")
    s.enemy(NUTLET, 21)
    s.enemy(NUTLET, 23)
    s.column(28, 3, "BRICK")
    s.row(28, 7, "G")
    s.log(33, 3)
    s.log(37, 4)
    s.row(41, 6, "***")

    s.enemy(BEETLE, 47)
    s.enemy(BEETLE, 52)
    s.row(55, 5, "bbbb bbbGbbb bbb")
    s.row(55, 4, "b              b")
    s.row(58, 2, "*********")
    s.enemy(NUTLET, 62, 4)
    s.enemy(NUTLET, 68)
    s.stairs(73, (1, 2, 3, 4))
    s.row(77, 6, "***")
    s.stairs(84, (4, 3, 2, 1))

    s.row(89, 7, "bbb?bbCbb")
    s.row(92, 3, "P")
    s.enemy(BEETLE, 100)
    s.enemy(BEETLE, 104)
    s.enemy(NUTLET, 108)
    s.enemy(NUTLET, 110)
    s.log(113, 2)
    s.log(117, 3)
    s.enemy(LIFT_Y, 121, 4)
    s.row(121, 2, "***")

    s.row(130, 7, "b?b?b")
    s.enemy(NUTLET, 134)
    s.enemy(BEETLE, 140)
    s.row(144, 6, "L")
    s.stairs(150, (1, 2, 3, 4, 5, 6))
    s.row(150, 3, "   ***")
    s.enemy(NUTLET, 158)
    s.finish(166, 171)
    for column in range(171, 184):
        s.put(column, 0, "EMPTY")
    return s


def canopy():
    s = Stage("1-3", 172, theme=2, time=300)
    s.checkpoint = 80
    s.ground(0, 14)
    for column, row in ((4, 2), (26, 1), (48, 3), (70, 2), (95, 1), (118, 3), (140, 2), (160, 1)):
        s.cloud(column, row, 2)
    s.row(8, 7, "b?b")

    s.canopy(16, 10, 5)
    s.canopy(23, 7, 4)
    s.row(24, 5, "**")
    s.canopy(29, 9, 6)
    s.enemy(NUTLET, 32, 8)
    s.canopy(37, 5, 5)
    s.row(38, 3, "***")
    s.canopy(44, 8, 3)
    s.canopy(49, 10, 8)
    s.enemy(BEETLE, 54, 9)
    s.enemy(LIFT_X, 58, 8)
    s.canopy(67, 6, 5)
    s.row(69, 2, "P")
    s.canopy(74, 9, 4)
    s.enemy(NUTLET, 76, 8)
    s.canopy(80, 11, 6)
    s.row(81, 9, "****")
    s.enemy(LIFT_Y, 88, 5)
    s.canopy(92, 4, 5)
    s.row(93, 2, "***")
    s.canopy(99, 8, 4)
    s.canopy(105, 6, 3)
    s.row(105, 3, "C")
    s.canopy(110, 10, 7)
    s.enemy(BEETLE, 112, 9)
    s.enemy(NUTLET, 115, 9)
    s.enemy(LIFT_X, 119, 7)
    s.canopy(128, 9, 5)
    s.row(129, 7, "***")
    s.canopy(135, 7, 3)
    s.canopy(140, 10, 4)

    s.ground(146, 172)
    s.stairs(149, (1, 2, 3, 4))
    s.enemy(NUTLET, 153)
    s.finish(160, 165)
    return s


STAGES = [meadow, hollow, canopy]
