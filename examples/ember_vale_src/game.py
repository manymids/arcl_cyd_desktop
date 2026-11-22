"""EMBER VALE: a small top-down RPG demo for ARCL CYD Desktop.

Walk the valley with the on-screen gamepad, talk to the villagers, open
chests and fight the monsters that appear in the fields. Everything is drawn
with the firmware's tile map view (cyd.tile_*, API level 6). Built by
build.py into game.mpy.
"""

import gc
import random
import time

import cyd
from micropython import const

# Tiles (art.TILES order; build.py checks them).
T_VOID = const(0)
T_GRASS = const(1)
T_FLOWERS = const(2)
T_TREE = const(3)
T_MOUNTAIN = const(4)
T_CAVE = const(5)
T_WATER = const(6)
T_BRIDGE = const(7)
T_PATH = const(8)
T_WALL = const(9)
T_FLOOR = const(10)
T_HOUSE = const(11)
T_ROOF = const(12)
T_DOOR = const(13)
T_CHEST = const(14)
T_CHEST_OPEN = const(15)
T_WELL = const(16)

# Sprite cells (art.SPRITES order).
S_HERO_DOWN_1 = const(0)
S_HERO_UP_1 = const(2)
S_HERO_SIDE_1 = const(4)
S_VILLAGER_1 = const(6)
S_ELDER_1 = const(8)
S_MOSSBACK_0 = const(10)
S_EMBERWING_0 = const(19)

FLIP_X = const(1)
FLIP_Y = const(2)
PAL_HERO = const(16)
PAL_VILLAGER = const(32)
PAL_VARIANT = const(48)

# Palette 0 colours used by the windows and the pad.
C_BLACK = const(0)
C_WHITE = const(1)
C_DARK = const(2)
C_LIGHT = const(3)
C_RED = const(12)
C_GOLD = const(13)

# Screen layout.
HERO_X = const(152)
HERO_Y = const(72)
PAD_X = const(60)        # centre of the direction pad
PAD_Y = const(166)

# Rectangle slots.
R_PAD_H = const(0)
R_PAD_V = const(1)
R_A = const(2)
R_B = const(3)
R_MESSAGE = const(4)
R_STATUS = const(5)
R_COMMAND = const(6)
R_FLASH = const(7)

# Text slots. Text is drawn over the rectangles, sprites under them, so the
# pad arrows and the menu cursor are text too.
X_PAD = const(0)         # A, B and the four arrows
X_MESSAGE = const(6)     # three lines
X_STATUS = const(9)      # three lines
X_COMMAND = const(12)    # four lines

# Directions.
DOWN = const(0)
UP = const(1)
LEFT = const(2)
RIGHT = const(3)
STEP = ((0, 1), (0, -1), (-1, 0), (1, 0))

APP_DIR = "/sd/apps/embervale/"

SOLID = bytearray(32)
for _tile in (T_VOID, T_TREE, T_MOUNTAIN, T_CAVE, T_WATER, T_WALL, T_HOUSE, T_ROOF, T_DOOR,
              T_CHEST, T_CHEST_OPEN, T_WELL):
    SOLID[_tile] = 1

MESSAGES = (
    ("ELDER: Welcome home, RUNE.", "Monsters wander the fields", "past the gate. Take care."),
    ("INNKEEPER: You look tired.", "Rest a while, it is free", "for heroes of the vale."),
    ("Did you see the old cave", "in the eastern hills? Nobody", "has come back from there."),
    ("The river bridge is the only", "way across. Mind the", "Emberwings at dusk."),
)

# name, first cell, palette flags, hp, attack, defence, exp, gold
MONSTERS = (
    ("MOSSBACK", S_MOSSBACK_0, PAL_HERO, 8, 5, 1, 3, 4),
    ("EMBERWING", S_EMBERWING_0, PAL_HERO, 12, 7, 2, 6, 7),
    ("RUSTBACK", S_MOSSBACK_0, PAL_VARIANT, 18, 9, 4, 11, 12),
    ("FROSTWING", S_EMBERWING_0, PAL_VARIANT, 24, 12, 5, 18, 20),
)
LEVELS = (0, 8, 22, 45, 80, 130, 200, 300, 450, 700)


class Quit(Exception):
    pass


# ---------------------------------------------------------------------------
# Assets

tile_cells = None
sprite_cells = None
palettes = None
palettes_shimmer = None
world = None
world_width = 0
world_height = 0
blank_map = bytearray(20 * 15)
start_column = 0
start_row = 0
people = []      # [kind, x, y, target_x, target_y, facing, wanders, message, home_x, home_y]


def load(gfx, world_data):
    global tile_cells, sprite_cells, palettes, palettes_shimmer, world, world_width, world_height
    global hero_x, hero_y
    view = memoryview(gfx)
    tile_count = gfx[4]
    sprite_count = gfx[5]
    palettes = view[8:136]
    palettes_shimmer = view[136:264]
    offset = 264
    tile_cells = view[offset:offset + tile_count * 128]
    offset += tile_count * 128
    sprite_cells = view[offset:offset + sprite_count * 128]
    global start_column, start_row
    world_width = world_data[4]
    world_height = world_data[5]
    start_column = world_data[6]
    start_row = world_data[7]
    hero_x = start_column * 16
    hero_y = start_row * 16
    count = world_data[8]
    size = world_width * world_height
    world = bytearray(memoryview(world_data)[9:9 + size])
    base = 9 + size
    for index in range(count):
        kind, column, row, wanders, message = world_data[base + index * 5:base + index * 5 + 5]
        people.append([kind, column * 16, row * 16, column * 16, row * 16, DOWN, wanders, message,
                       column, row])


def tile_at(column, row):
    if column < 0 or row < 0 or column >= world_width or row >= world_height:
        return T_VOID
    return world[row * world_width + column]


def set_tile(column, row, tile):
    world[row * world_width + column] = tile


# ---------------------------------------------------------------------------
# Hero and progress

hero_x = 0
hero_y = 0
target_x = 0
target_y = 0
facing = DOWN
steps_since_battle = 0

level = 1
hp = 15
max_hp = 15
mp = 6
max_mp = 6
attack = 5
defence = 3
exp = 0
gold = 0
herbs = 1


# ---------------------------------------------------------------------------
# Input: the on-screen pad

pad_direction = None
pad_a = False
pad_b = False
a_edge = False
b_edge = False
direction_edge = False
_last_direction = None
_last_a = False
_last_b = False
_repeat_ticks = 0


def read_pad():
    global pad_direction, pad_a, pad_b, a_edge, b_edge, direction_edge
    global _last_direction, _last_a, _last_b, _repeat_ticks
    pressed, x, y = cyd.touch()
    direction = None
    a = b = False
    if pressed and y < 212:
        dx = x - PAD_X
        dy = y - PAD_Y
        if -60 <= dx <= 60 and -56 <= dy <= 56 and (dx * dx + dy * dy) > 64:
            if abs(dx) > abs(dy):
                direction = RIGHT if dx > 0 else LEFT
            else:
                direction = DOWN if dy > 0 else UP
        elif x >= 258 and y >= 128:
            a = True
        elif 192 <= x < 258 and y >= 140:
            b = True
    direction_edge = direction is not None and direction != _last_direction
    # Holding a direction in a menu repeats it.
    if direction is not None and not direction_edge:
        _repeat_ticks += 1
        if _repeat_ticks >= 10:
            _repeat_ticks = 6
            direction_edge = True
    else:
        _repeat_ticks = 0
    a_edge = a and not _last_a
    b_edge = b and not _last_b
    pad_direction, pad_a, pad_b = direction, a, b
    _last_direction, _last_a, _last_b = direction, a, b


def consume_edges():
    """A press belongs to the first tick of a frame, not to every tick it covers."""
    global a_edge, b_edge, direction_edge
    a_edge = b_edge = direction_edge = False


def draw_pad():
    cyd.tile_rect(R_PAD_H, PAD_X - 54, PAD_Y - 18, 108, 36, C_DARK, C_WHITE)
    cyd.tile_rect(R_PAD_V, PAD_X - 18, PAD_Y - 44, 36, 88, C_DARK, -1)
    cyd.tile_rect(R_A, 264, 150, 50, 50, C_LIGHT if pad_a else C_DARK, C_WHITE)
    cyd.tile_rect(R_B, 206, 162, 46, 42, C_LIGHT if pad_b else C_DARK, C_WHITE)


def pad_labels():
    cyd.tile_text(X_PAD, 283, 168, "A", C_WHITE, 2)
    cyd.tile_text(X_PAD + 1, 223, 176, "B", C_WHITE, 2)
    cyd.tile_text(X_PAD + 2, PAD_X - 5, PAD_Y - 37, "^", C_WHITE, 2)
    cyd.tile_text(X_PAD + 3, PAD_X - 5, PAD_Y + 24, "v", C_WHITE, 2)
    cyd.tile_text(X_PAD + 4, PAD_X - 46, PAD_Y - 7, "<", C_WHITE, 2)
    cyd.tile_text(X_PAD + 5, PAD_X + 36, PAD_Y - 7, ">", C_WHITE, 2)


# ---------------------------------------------------------------------------
# Windows

message_pages = []
message_page = 0
message_chars = 0
message_done = None     # called when the last page closes
message_y = 4


def show_message(pages, done=None, y=4):
    """Top of the screen on the map, below the monster in battle."""
    global message_pages, message_page, message_chars, message_done, message_y
    message_pages = pages
    message_page = 0
    message_chars = 0
    message_done = done
    message_y = y
    cyd.tile_rect(R_MESSAGE, 4, y, 312, 50, C_BLACK, C_WHITE)


def message_tick():
    """Types the current page out; returns True while a message is open."""
    global message_page, message_chars, message_pages
    if not message_pages:
        return False
    lines = message_pages[message_page]
    total = sum(len(line) for line in lines)
    if message_chars < total:
        message_chars += 2
        if a_edge:
            message_chars = total
    elif a_edge:
        message_page += 1
        message_chars = 0
        if message_page >= len(message_pages):
            close_message()
            return True     # the press that closed it is used up
    left = message_chars
    ready = message_chars >= total
    for index in range(3):
        text = lines[index] if index < len(lines) else ""
        shown = text[:max(0, left)]
        left -= len(text)
        if ready and index == len(lines) - 1 and (ticks >> 4) & 1:
            shown = shown + " v"      # waiting for A
        cyd.tile_text(X_MESSAGE + index, 14, message_y + 8 + index * 13, shown, C_WHITE, 1)
    return True


def close_message():
    global message_pages, message_done
    message_pages = []
    cyd.tile_rect(R_MESSAGE, 0, 0, 0, 0, 0, -1)
    for index in range(3):
        cyd.tile_text(X_MESSAGE + index, 0, 0, "")
    done = message_done
    message_done = None
    if done:
        done()


def show_status(visible, x=4, y=4, alarm=False):
    if not visible:
        cyd.tile_rect(R_STATUS, 0, 0, 0, 0, 0, -1)
        for index in range(3):
            cyd.tile_text(X_STATUS + index, 0, 0, "")
        return
    cyd.tile_rect(R_STATUS, x, y, 100, 50, C_BLACK, C_RED if alarm else C_WHITE)
    lines = ("RUNE LV%d G%d" % (level, gold), "HP %3d/%d" % (hp, max_hp), "MP %3d/%d" % (mp, max_mp))
    for index in range(3):
        cyd.tile_text(X_STATUS + index, x + 10, y + 8 + index * 12, lines[index], C_RED if index == 1 and hp * 4 <= max_hp else C_WHITE, 1)


def show_commands(items, x=0, y=0, cursor=0):
    """A command window with a ">" cursor; None closes it."""
    if not items:
        cyd.tile_rect(R_COMMAND, 0, 0, 0, 0, 0, -1)
        for index in range(4):
            cyd.tile_text(X_COMMAND + index, 0, 0, "")
        return
    cyd.tile_rect(R_COMMAND, x, y, 104, 16 + len(items) * 12, C_BLACK, C_WHITE)
    for index in range(4):
        text = ""
        if index < len(items):
            text = ("> " if index == cursor and (ticks >> 3) & 3 else "  ") + items[index]
        cyd.tile_text(X_COMMAND + index, x + 8, y + 8 + index * 12, text, C_WHITE, 1)


# ---------------------------------------------------------------------------
# Field

ticks = 0
status_open = False
shimmer = False


def enter_field():
    cyd.tile_begin(palettes, tile_cells, sprite_cells, world, world_width, world_height, 0)
    clear_windows()
    pad_labels()


def clear_windows():
    for slot in range(4, 8):
        cyd.tile_rect(slot, 0, 0, 0, 0, 0, -1)
    for slot in range(16):
        cyd.tile_text(slot, 0, 0, "")


def blocked(column, row, mover=None):
    if SOLID[tile_at(column, row)]:
        return True
    if mover is not None and column * 16 == target_x and row * 16 == target_y:
        return True
    for person in people:
        if person is mover:
            continue
        if (person[3] >> 4 == column and person[4] >> 4 == row) or (person[1] >> 4 == column and person[2] >> 4 == row):
            return True
    return False


def person_at(column, row):
    for person in people:
        if person[3] >> 4 == column and person[4] >> 4 == row:
            return person
    return None


def field_tick():
    """One 60 Hz step on the map. Returns a monster number to fight, or None."""
    global hero_x, hero_y, target_x, target_y, facing, status_open, steps_since_battle
    if message_tick():
        return None
    if b_edge:
        status_open = not status_open
        show_status(status_open, 4, 58 if message_pages else 4)

    moving = hero_x != target_x or hero_y != target_y
    if moving:
        hero_x += (target_x > hero_x) - (target_x < hero_x)
        hero_y += (target_y > hero_y) - (target_y < hero_y)
        if hero_x == target_x and hero_y == target_y:
            steps_since_battle += 1
            tile = tile_at(hero_x >> 4, hero_y >> 4)
            if tile in (T_GRASS, T_FLOWERS) and steps_since_battle > 3 and random.getrandbits(4) == 0:
                return pick_monster()
    elif pad_direction is not None and not status_open:
        facing = pad_direction
        dx, dy = STEP[facing]
        column = (hero_x >> 4) + dx
        row = (hero_y >> 4) + dy
        if not blocked(column, row):
            target_x = column * 16
            target_y = row * 16
    elif a_edge:
        examine()

    for person in people:
        if person[1] != person[3] or person[2] != person[4]:
            person[1] += (person[3] > person[1]) - (person[3] < person[1])
            person[2] += (person[4] > person[2]) - (person[4] < person[2])
        elif person[6] and not message_pages and random.getrandbits(7) == 0:
            direction = random.getrandbits(2)
            dx, dy = STEP[direction]
            column = (person[1] >> 4) + dx
            row = (person[2] >> 4) + dy
            person[5] = direction
            near_home = abs(column - person[8]) <= 3 and abs(row - person[9]) <= 3
            if near_home and not blocked(column, row, person):
                person[3] = column * 16
                person[4] = row * 16
    return None


def examine():
    global gold, herbs, hp, mp
    dx, dy = STEP[facing]
    column = (hero_x >> 4) + dx
    row = (hero_y >> 4) + dy
    person = person_at(column, row)
    tile = tile_at(column, row)
    if person is not None:
        person[5] = (UP, DOWN, RIGHT, LEFT)[facing]
        number = person[7]
        if number == 1:
            def rest():
                global hp, mp
                hp = max_hp
                mp = max_mp
                show_message([("RUNE rested at the inn.", "HP and MP are restored.")])
            show_message([MESSAGES[number]], rest)
            return
        else:
            show_message([MESSAGES[number]])
    elif tile == T_CHEST:
        set_tile(column, row, T_CHEST_OPEN)
        if (column + row) % 2:
            gold += 25
            show_message([("RUNE opened the chest.", "Inside: 25 gold coins.")])
        else:
            herbs += 1
            show_message([("RUNE opened the chest.", "Inside: a healing HERB.")])
    elif tile == T_CHEST_OPEN:
        show_message([("The chest is empty.",)])
    elif tile == T_CAVE:
        show_message([("A cold wind blows out of", "the cave. (Not open in", "this demo.)")])
    elif tile == T_DOOR:
        show_message([("The door is locked.",)])
    elif tile == T_WELL:
        show_message([("Deep water glints far below.",)])
    elif tile == T_WATER:
        show_message([("The river runs fast and cold.",)])
    else:
        show_message([("RUNE looks around.", "Nothing special here.")])


def pick_monster():
    top = min(len(MONSTERS), 2 + level // 2)
    return random.getrandbits(8) % top


def field_draw():
    global shimmer
    camera_x = hero_x - HERO_X
    camera_y = hero_y - HERO_Y
    if ticks % 40 == 0:
        shimmer = not shimmer
        cyd.tile_palette(palettes_shimmer if shimmer else palettes)
    cyd.tile_frame(camera_x, camera_y)
    sprite = cyd.tile_sprite
    step = (ticks >> 4) & 1
    for kind, x, y, _, _, direction, _, _, _, _ in people:
        sx = x - camera_x
        sy = y - camera_y
        if -16 < sx < 320 and -16 < sy < 240:
            if kind == 1:
                sprite(S_ELDER_1 + step, sx, sy, PAL_HERO)
            else:
                sprite(S_VILLAGER_1 + step, sx, sy, PAL_VILLAGER)
    if facing == DOWN:
        sprite(S_HERO_DOWN_1 + step, HERO_X, HERO_Y, PAL_HERO)
    elif facing == UP:
        sprite(S_HERO_UP_1 + step, HERO_X, HERO_Y, PAL_HERO)
    else:
        sprite(S_HERO_SIDE_1 + step, HERO_X, HERO_Y, PAL_HERO | (FLIP_X if facing == LEFT else 0))
    draw_pad()
    return cyd.tile_present()


# ---------------------------------------------------------------------------
# Battle

def battle(number):
    global hp, mp, exp, gold, level, max_hp, max_mp, attack, defence, herbs, steps_since_battle
    name, first_cell, flags, monster_hp, monster_attack, monster_defence, reward_exp, reward_gold = MONSTERS[number]
    steps_since_battle = 0

    # A few flashes, then the battle screen.
    for frame in range(8):
        cyd.tile_rect(R_FLASH, 0, 0, 320, 240, C_WHITE if frame & 1 else C_BLACK, -1)
        present_frame(field_draw)
    cyd.tile_begin(palettes, tile_cells, sprite_cells, blank_map, 20, 15, 0)
    clear_windows()
    pad_labels()

    state = {"hit": 0, "hurt": 0, "gone": False}
    pages = [("A %s draws near!" % name,)]
    menu = None
    cursor = 0
    outcome = None

    def enemy_turn():
        global hp
        damage = max(1, monster_attack - defence // 2 + random.getrandbits(2) - 1)
        hp = max(0, hp - damage)
        state["hurt"] = 16
        return ("The %s attacks!" % name, "RUNE takes %d damage." % damage)

    while True:
        if pages:
            show_message(pages, None, 70)
            pages = []
        if message_pages:
            message_tick()
        elif outcome is not None:
            break
        elif hp <= 0:
            pages = [("RUNE has fallen...",), ("RUNE wakes up in the village.",)]
            outcome = "lost"
        elif monster_hp <= 0:
            gold += reward_gold
            exp += reward_exp
            pages = [("The %s is defeated!" % name,), ("RUNE gains %d EXP" % reward_exp, "and %d gold." % reward_gold)]
            while level < len(LEVELS) and exp >= LEVELS[level]:
                level += 1
                max_hp += 4 + random.getrandbits(2)
                max_mp += 2
                attack += 2
                defence += 1
                pages.append(("RUNE reached level %d!" % level, "HP %d  MP %d" % (max_hp, max_mp)))
            outcome = "won"
        else:
            if menu is None:
                menu = "main"
                cursor = 0
            items = ("FIGHT", "SPELL", "HERB x%d" % herbs, "RUN") if menu == "main" else ("HEAL   3MP", "SPARK  2MP", "BACK")
            if direction_edge and pad_direction in (UP, DOWN):
                cursor = (cursor + (1 if pad_direction == DOWN else -1)) % len(items)
            if b_edge and menu == "spell":
                menu = "main"
                cursor = 1
            elif a_edge:
                choice = items[cursor]
                if menu == "main" and choice == "FIGHT":
                    damage = max(1, attack - monster_defence // 2 + random.getrandbits(3) - 2)
                    if random.getrandbits(4) == 0:
                        damage *= 2
                        pages = [("RUNE strikes a mighty blow!", "%d damage to the %s." % (damage, name))]
                    else:
                        pages = [("RUNE attacks!", "%d damage to the %s." % (damage, name))]
                    monster_hp -= damage
                    state["hit"] = 16
                    if monster_hp > 0:
                        pages.append(enemy_turn())
                    menu = None
                elif menu == "main" and choice == "SPELL":
                    menu = "spell"
                    cursor = 0
                elif menu == "main" and choice.startswith("HERB"):
                    if herbs:
                        herbs -= 1
                        healed = min(max_hp - hp, 12)
                        hp += healed
                        pages = [("RUNE eats a HERB.", "%d HP restored." % healed), enemy_turn()]
                    else:
                        pages = [("RUNE has no HERBs.",)]
                    menu = None
                elif menu == "main" and choice == "RUN":
                    if random.getrandbits(2):
                        pages = [("RUNE ran away!",)]
                        outcome = "ran"
                    else:
                        pages = [("RUNE could not escape!",), enemy_turn()]
                    menu = None
                elif choice.startswith("HEAL"):
                    if mp >= 3:
                        mp -= 3
                        healed = min(max_hp - hp, 10 + random.getrandbits(3))
                        hp += healed
                        pages = [("RUNE casts HEAL.", "%d HP restored." % healed), enemy_turn()]
                    else:
                        pages = [("Not enough MP.",)]
                    menu = None
                elif choice.startswith("SPARK"):
                    if mp >= 2:
                        mp -= 2
                        damage = 6 + random.getrandbits(3)
                        monster_hp -= damage
                        state["hit"] = 16
                        pages = [("RUNE casts SPARK!", "%d damage to the %s." % (damage, name))]
                        if monster_hp > 0:
                            pages.append(enemy_turn())
                    else:
                        pages = [("Not enough MP.",)]
                    menu = None
                elif choice == "BACK":
                    menu = "main"
                    cursor = 1
            if menu is not None and not pages:
                show_commands(items, 212, 4, cursor)
            else:
                show_commands(None)
        if message_pages or outcome is not None or pages:
            show_commands(None)

        if monster_hp <= 0 and not message_pages:
            state["gone"] = True

        def draw():
            cyd.tile_frame(0, (random.getrandbits(3) - 4) if state["hurt"] > 8 else 0)
            show_status(True, 4, 4, state["hurt"] > 0)
            if not state["gone"] and not (state["hit"] and (state["hit"] >> 1) & 1):
                for index in range(9):
                    cyd.tile_sprite(first_cell + index, 136 + (index % 3) * 16, 18 + (index // 3) * 16, flags)
            draw_pad()
            return cyd.tile_present()

        present_frame(draw)
        if state["hit"]:
            state["hit"] -= 1
        if state["hurt"]:
            state["hurt"] -= 1

    show_commands(None)
    if outcome == "lost":
        hp = max_hp
        mp = max_mp
        gold //= 2
        return_to_village()
    return outcome


def return_to_village():
    global hero_x, hero_y, target_x, target_y, facing
    hero_x = target_x = start_column * 16
    hero_y = target_y = start_row * 16
    facing = LEFT


# ---------------------------------------------------------------------------
# Main loop

_last_ms = 0
_backlog = 0


def present_frame(draw):
    """Read the pad, show a frame, and count the 60 Hz ticks it covered."""
    global _last_ms, _backlog, ticks
    if not cyd.update():
        raise Quit()
    read_pad()
    if not draw():
        raise Quit()
    now = time.ticks_ms()
    _backlog = min(_backlog + time.ticks_diff(now, _last_ms) * 3, 150)
    _last_ms = now
    count = 0
    while _backlog >= 50:
        _backlog -= 50
        count += 1
    ticks += max(1, count)
    return max(1, count)


def title():
    cyd.tile_begin(palettes, tile_cells, sprite_cells, blank_map, 20, 15, 0)
    clear_windows()
    pad_labels()
    cyd.tile_rect(R_MESSAGE, 40, 34, 240, 64, C_BLACK, C_GOLD)
    cyd.tile_text(X_MESSAGE, 76, 46, "EMBER VALE", C_GOLD, 3)
    cyd.tile_text(X_MESSAGE + 1, 94, 76, "A TINY RPG DEMO", C_WHITE, 1)
    while True:
        def draw():
            cyd.tile_frame(0, 0)
            cyd.tile_text(X_MESSAGE + 2, 118, 132, "TOUCH A" if (ticks >> 5) & 1 else "", C_WHITE, 1)
            cyd.tile_sprite(S_HERO_DOWN_1 + ((ticks >> 4) & 1), 152, 108, PAL_HERO)
            draw_pad()
            return cyd.tile_present()
        present_frame(draw)
        if a_edge:
            return


def main(gfx=None, world_data=None):
    global target_x, target_y, _last_ms
    cyd.no_time_limit()
    if gfx is None:
        with open(APP_DIR + "gfx.bin", "rb") as source:
            gfx = source.read()
    if world_data is None:
        with open(APP_DIR + "world.bin", "rb") as source:
            world_data = source.read()
    load(gfx, world_data)
    world_data = None
    target_x, target_y = hero_x, hero_y
    gc.collect()
    _last_ms = time.ticks_ms()
    try:
        title()
        enter_field()
        show_message([("RUNE, the elder wants to", "see you. His house is the", "one to the west.")])
        while True:
            fight = None
            def draw():
                return field_draw()
            count = present_frame(draw)
            for _ in range(count):
                fight = field_tick()
                consume_edges()
                if fight is not None:
                    break
            if fight is not None:
                if status_open:
                    show_status(False)
                battle(fight)
                enter_field()
                if status_open:
                    show_status(True)
                gc.collect()
    except Quit:
        pass
