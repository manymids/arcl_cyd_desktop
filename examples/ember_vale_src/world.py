"""The EMBER VALE world map: a walled village, fields, a river and hills.

Built with small placement helpers; game.py gets the tile map, the people
and the chests from world.bin.
"""

from art import TILES

T = {name: index for index, (name, _) in enumerate(TILES)}
WIDTH = 48
HEIGHT = 40

# People: kind (0 villager, 1 elder), column, row, wanders, message number.
# Messages live in game.py so the text is next to the code that shows it.
VILLAGER = 0
ELDER = 1


class World:
    def __init__(self):
        self.map = bytearray([T["GRASS"]] * (WIDTH * HEIGHT))
        self.people = []
        self.start = (0, 0)

    def put(self, column, row, name):
        if 0 <= column < WIDTH and 0 <= row < HEIGHT:
            self.map[row * WIDTH + column] = T[name]

    def fill(self, column, row, width, height, name):
        for y in range(row, row + height):
            for x in range(column, column + width):
                self.put(x, y, name)

    def frame(self, column, row, width, height, name):
        for x in range(column, column + width):
            self.put(x, row, name)
            self.put(x, row + height - 1, name)
        for y in range(row, row + height):
            self.put(column, y, name)
            self.put(column + width - 1, y, name)

    def house(self, column, row, width):
        self.fill(column, row, width, 2, "ROOF")
        self.fill(column, row + 2, width, 2, "HOUSE")
        self.put(column + width // 2, row + 3, "DOOR")


def build():
    w = World()
    # Border of hills and forest around the whole valley.
    w.frame(0, 0, WIDTH, HEIGHT, "MOUNTAIN")
    w.frame(1, 1, WIDTH - 2, HEIGHT - 2, "TREE")

    # Forest patches and meadows of flowers.
    for column, row, width, height in ((24, 3, 6, 4), (36, 26, 8, 5), (4, 28, 7, 6), (30, 14, 3, 5)):
        w.fill(column, row, width, height, "TREE")
    for column, row in ((6, 22), (7, 23), (21, 20), (40, 9), (41, 10), (14, 33), (27, 34), (35, 6)):
        w.put(column, row, "FLOWERS")

    # Hills in the north-east with a cave.
    w.fill(36, 2, 10, 5, "MOUNTAIN")
    w.fill(40, 7, 6, 2, "MOUNTAIN")
    w.put(40, 6, "CAVE")

    # A river running north to south, with one bridge.
    for row in range(2, HEIGHT - 2):
        column = 22 + (row // 9) % 2
        w.put(column, row, "WATER")
        w.put(column + 1, row, "WATER")
    for column in range(20, 27):
        w.put(column, 18, "PATH")
    bridge_column = 22 + (18 // 9) % 2
    w.put(bridge_column, 18, "BRIDGE")
    w.put(bridge_column + 1, 18, "BRIDGE")
    for column in range(27, 40):
        w.put(column, 18, "PATH")
    for row in range(8, 18):
        w.put(39, row, "PATH")

    # The village: stone wall with a gate on the east side.
    vx, vy, vw, vh = 3, 3, 16, 14
    w.fill(vx, vy, vw, vh, "FLOOR")
    w.frame(vx, vy, vw, vh, "WALL")
    w.put(vx + vw - 1, 11, "FLOOR")
    for column in range(vx + vw, 21):
        w.put(column, 11, "PATH")
    for row in range(11, 19):
        w.put(20, row, "PATH")
    w.house(5, 4, 5)      # the elder's house
    w.house(12, 4, 5)     # the inn
    w.house(5, 11, 4)
    w.put(12, 10, "WELL")
    w.put(16, 13, "CHEST")
    w.put(4, 15, "CHEST")
    w.put(40, 33, "CHEST")

    w.people += [
        (ELDER, 7, 8, False, 0),
        (VILLAGER, 14, 8, False, 1),     # innkeeper
        (VILLAGER, 10, 13, True, 2),
        (VILLAGER, 15, 11, True, 3),
    ]
    w.start = (17, 11)
    return w
