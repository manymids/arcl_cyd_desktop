"""PIP QUEST: a side-scrolling platformer for ARCL CYD Desktop.

Game logic in MicroPython; the firmware's tile map view (cyd.tile_*) draws
the stage, sprites and text. Built by build.py into game.mpy.

Keys: arrows (or A/D) move, Z or Space jumps, X or Shift dashes and throws
embers, Down crouches, Enter pauses and starts.
"""

import gc
import time

import cyd
from micropython import const

# Tile numbers (art.TILES order; build.py checks them).
T_EMPTY = const(0)
T_GRASS = const(1)
T_BRICK = const(2)
T_GEM_BLOCK = const(3)
T_USED = const(4)
T_HARD = const(5)
T_LOG_RIM_L = const(6)
T_LOG_R = const(9)
T_GEM = const(10)
T_DIRT = const(11)
T_POLE_TOP = const(18)
T_POLE = const(19)
T_HIDDEN_LIFE = const(27)
T_GEM_BLOCK_POWER = const(28)
T_BRICK_GEMS = const(29)
T_BRICK_POWER = const(30)
T_BRICK_COMET = const(31)
T_CANOPY_L = const(32)
T_CANOPY_R = const(34)
T_BUMPING = const(36)

# Sprite cells (art.SPRITES order).
S_PIP_STAND = const(0)
S_PIP_WALK1 = const(1)
S_PIP_SKID = const(4)
S_PIP_JUMP = const(5)
S_PIP_HURT = const(6)
S_BIG_HEAD = const(7)
S_BIG_STAND = const(8)
S_BIG_WALK1 = const(9)
S_BIG_JUMP = const(12)
S_NUT_1 = const(13)
S_NUT_2 = const(14)
S_NUT_FLAT = const(15)
S_BEETLE_1 = const(16)
S_BEETLE_2 = const(17)
S_SHELL = const(18)
S_SPROUT = const(19)
S_EMBER_FLOWER = const(20)
S_COMET = const(21)
S_HEART = const(22)
S_GEM = const(23)
S_GEM_NARROW = const(24)
S_GEM_EDGE = const(25)
S_EMBER = const(26)
S_PUFF = const(27)
S_DEBRIS = const(28)
S_BANNER = const(29)
S_LIFT = const(30)
S_BUMP_BRICK = const(31)
S_BUMP_GEM_BLOCK = const(32)
S_BUMP_USED = const(33)

ROWS = const(13)
MAP_CAPACITY = const(2600)   # widest stage x ROWS; build.py checks it
TOP = const(32)          # screen row of map row 0
FLIP_X = const(1)
FLIP_Y = const(2)
BEHIND = const(4)
PAL_CHAR = const(16)
PAL_EMBER = const(32)
PAL_COMET = const(48)

# Movement, in 1/4096 pixel per frame at 60 frames a second.
MIN_WALK = const(0x130)
WALK_ACCEL = const(0x98)
RUN_ACCEL = const(0xE4)
RELEASE_DECEL = const(0xD0)
SKID_DECEL = const(0x1A0)
MAX_WALK = const(0x1900)
MAX_RUN = const(0x2900)
SKID_TURN = const(0x900)
MAX_FALL = const(0x4800)
FALL_CAP = const(0x4000)
STOMP_BOUNCE = const(0x4000)
ENEMY_SPEED = const(0x800)
SHELL_SPEED = const(0x3000)
ITEM_SPEED = const(0x1000)
EMBER_SPEED = const(0x4000)

# Object kinds from levels.py, and runtime kinds.
NUTLET = const(1)
BEETLE = const(2)
LIFT_X = const(3)
LIFT_Y = const(4)
SPROUT = const(1)
FLOWER = const(2)
COMET = const(3)
HEART = const(4)

# Enemy states.
WALK = const(0)
FLAT = const(1)
FALLING = const(2)
SHELL = const(3)
SHELL_MOVING = const(4)

# Player states.
PLAY = const(0)
CHANGE = const(1)
DYING = const(2)
POLE = const(3)
WALK_OUT = const(4)
TALLY = const(5)

STOMP_POINTS = (100, 200, 400, 500, 800, 1000, 2000, 4000, 5000, 8000)
BUMP_RISE = (0, 3, 5, 6, 6, 5, 3, 1)

APP_DIR = "/sd/apps/pipquest/"

# Solidity by tile: 1 solid, 2 solid only when hit from below.
SOLID = bytearray(64)
for _tile in (T_GRASS, T_BRICK, T_GEM_BLOCK, T_USED, T_HARD, 6, 7, 8, 9, T_DIRT,
              T_GEM_BLOCK_POWER, T_BRICK_GEMS, T_BRICK_POWER, T_BRICK_COMET, 32, 33, 34, T_BUMPING):
    SOLID[_tile] = 1
SOLID[T_HIDDEN_LIFE] = 2


class Quit(Exception):
    pass


# ---------------------------------------------------------------------------
# Assets

gfx = None
tile_cells = None
sprite_cells = None
palettes = []    # one bytes object of four palettes per theme


def load_gfx(data=None):
    global gfx, tile_cells, sprite_cells
    if data is None:
        with open(APP_DIR + "gfx.bin", "rb") as source:
            data = source.read()
    gfx = data
    themes, tile_count, sprite_count = gfx[4], gfx[5], gfx[6]
    view = memoryview(gfx)
    offset = 8
    for _ in range(themes):
        palettes.append(view[offset:offset + 128])
        offset += 128
    tile_cells = view[offset:offset + tile_count * 128]
    offset += tile_count * 128
    sprite_cells = view[offset:offset + sprite_count * 128]


def dark_palette(theme):
    colors = bytearray(palettes[theme])
    colors[0] = 0
    colors[1] = 0
    return colors


# ---------------------------------------------------------------------------
# Input

keys_held = ()
jump_edge = False
dash_edge = False
_jump_was = False
_dash_was = False


def read_input():
    global keys_held, jump_edge, dash_edge, _jump_was, _dash_was
    keys = cyd.keys_down()
    keys_held = keys
    jump = "z" in keys or " " in keys or "k" in keys
    dash = "x" in keys or "SHIFT" in keys or "j" in keys
    jump_edge = jump and not _jump_was
    dash_edge = dash and not _dash_was
    _jump_was = jump
    _dash_was = dash
    pressed = None
    while True:
        key = cyd.key()
        if key is None:
            break
        if key in ("ENTER", "ESC", "p"):
            pressed = key
    return pressed


def held(*names):
    for name in names:
        if name in keys_held:
            return True
    return False


# ---------------------------------------------------------------------------
# Game state

score = 0
gems = 0
lives = 3
stage_number = 1
lap = 0
top_score = 0

tilemap = None
width = 0
stage_name = "1-1"
stage_theme = 0
stage_time = 400
goal_column = 0
checkpoint_column = 0
objects = b""
object_count = 0
checkpoint_reached = False

camera = 0
next_object = 0
time_left = 400
time_ticks = 0
frame_count = 0

# Player
px = 0
py = 0
vx = 0
vy = 0
big = False
ember = False
face = 1
on_ground = True
hold_gravity = 0x200
fall_gravity = 0x700
air_max = MAX_WALK
jump_speed = 0
run_timer = 0
crouch = False
skid = False
hurt_timer = 0
comet_timer = 0
anim_step = 0
anim_timer = 0
state = PLAY
state_timer = 0
stomp_chain = 0
lift_under = None
banner_y = 0
hidden = False

enemies = []     # [kind, x, y, vx, vy, state, timer]
items = []       # [kind, x, y, vx, vy, rising]
embers = []      # [x, y, vx, vy]
effects = []     # [cell, x, y, vx, vy, timer, flags]  pixels
bumps = []       # [column, row, timer, result, cell]
lifts = []       # [kind, x, y, origin, direction, dx, dy] pixels
popups = [None, None, None, None]
popup_next = 0
brick_gems = {}


def tile_at(column, row):
    if column < 0 or column >= width:
        return T_HARD
    if row < 0 or row >= ROWS:
        return T_EMPTY
    return tilemap[row * width + column]


def solid_at(x, y):
    """Is world pixel (x, y) inside a fully solid tile?"""
    return SOLID[tile_at(x >> 4, y >> 4)] == 1


def set_tile(column, row, tile):
    if 0 <= column < width and 0 <= row < ROWS:
        tilemap[row * width + column] = tile


def add_score(points, x=None, y=None):
    global score, lives
    if points < 0:
        lives = min(lives + 1, 9)
        if x is not None:
            popup("1UP", x, y)
        return
    score += points
    if x is not None:
        popup(str(points), x, y)


def popup(text, x, y):
    global popup_next
    popups[popup_next] = [x, y, 45, text]
    popup_next = (popup_next + 1) % 4


def add_gem():
    global gems
    gems += 1
    add_score(200)
    if gems >= 100:
        gems -= 100
        add_score(-1)


# ---------------------------------------------------------------------------
# Stage loading

stage_header = bytearray(17)


def load_stage(number):
    """Reads a stage into the one map buffer. With Bluetooth on the heap is
    small and soon fragmented, so nothing here allocates more than the objects."""
    global width, stage_name, stage_theme, stage_time, goal_column
    global checkpoint_column, objects, object_count
    objects = b""
    gc.collect()
    data = stage_header
    with open(APP_DIR + "stage%d.bin" % number, "rb") as source:
        source.readinto(data)
        width = data[4] | data[5] << 8
        stage_theme = data[6]
        stage_time = data[7] | data[8] << 8
        goal_column = data[9] | data[10] << 8
        checkpoint_column = data[11] | data[12] << 8
        object_count = data[13]
        stage_name = "%c-%c" % (data[14], data[16])
        source.readinto(memoryview(tilemap)[:width * ROWS])
        objects = source.read(object_count * 3)


def start_stage():
    global camera, next_object, time_left, time_ticks, px, py, vx, vy, face, on_ground
    global hurt_timer, comet_timer, state, state_timer, crouch, run_timer, stomp_chain
    global lift_under, banner_y, hidden, popup_next
    load_stage(stage_number)
    start = checkpoint_column if checkpoint_reached else 3
    camera = max(0, start * 16 - 48)
    px = start * 16 << 12
    py = (11 * 16 - (32 if big else 16)) << 12
    vx = vy = 0
    face = 1
    on_ground = True
    hurt_timer = comet_timer = 0
    state = PLAY
    state_timer = 0
    crouch = False
    run_timer = 0
    stomp_chain = 0
    lift_under = None
    banner_y = 2 * 16 + 4
    hidden = False
    time_left = stage_time
    time_ticks = 0
    del enemies[:]
    del items[:]
    del embers[:]
    del effects[:]
    del bumps[:]
    del lifts[:]
    brick_gems.clear()
    for index in range(4):
        popups[index] = None
    popup_next = 0
    next_object = 0
    # Objects already behind the start stay unspawned.
    while next_object < object_count and objects[next_object * 3 + 1] * 16 < camera:
        next_object += 1
    cyd.tile_begin(palettes[stage_theme], tile_cells, sprite_cells, tilemap, width, ROWS, TOP)
    for slot in range(8):
        cyd.tile_text(slot, 0, 0, "")
    cyd.tile_text(0, 24, 8, "PIP")
    cyd.tile_text(1, 176, 8, "WORLD   TIME")
    gc.collect()


# ---------------------------------------------------------------------------
# Player

def hitbox_top():
    if not big or crouch:
        return 16 + 3 if big else 3
    return 4


def player_height():
    return 32 if big else 16


def start_jump():
    global vy, hold_gravity, fall_gravity, air_max, jump_speed, on_ground, lift_under
    speed = abs(vx)
    if speed < 0x1000:
        vy, hold_gravity, fall_gravity = -0x4000, 0x200, 0x700
    elif speed < 0x2500:
        vy, hold_gravity, fall_gravity = -0x4000, 0x1E0, 0x600
    else:
        vy, hold_gravity, fall_gravity = -0x5000, 0x280, 0x900
    air_max = MAX_RUN if speed >= MAX_WALK else MAX_WALK
    jump_speed = speed
    on_ground = False
    lift_under = None


def player_tick(first_tick):
    global px, py, vx, vy, face, on_ground, run_timer, crouch, skid, hurt_timer
    global comet_timer, anim_step, anim_timer, stomp_chain, lift_under
    left = held("LEFT", "a")
    right = held("RIGHT", "d")
    down = held("DOWN", "s")
    jump = held("z", " ", "k")
    dash = held("x", "SHIFT", "j")
    direction = (1 if right else 0) - (1 if left else 0)

    if hurt_timer:
        hurt_timer -= 1
    if comet_timer:
        comet_timer -= 1

    skid = False
    if on_ground:
        crouch = big and down
        if dash:
            run_timer = 10
        elif run_timer:
            run_timer -= 1
        limit = MAX_RUN if run_timer else MAX_WALK
        if crouch:
            direction = 0
        if direction == 0:
            if vx > 0:
                vx = max(0, vx - RELEASE_DECEL)
            elif vx < 0:
                vx = min(0, vx + RELEASE_DECEL)
        elif vx * direction < 0:
            skid = True
            if abs(vx) < SKID_TURN:
                vx = 0
                face = direction
            else:
                vx += SKID_DECEL * direction
        else:
            face = direction
            speed = abs(vx)
            if speed < MIN_WALK:
                vx = MIN_WALK * direction
            elif speed < limit:
                vx += (RUN_ACCEL if run_timer else WALK_ACCEL) * direction
                if abs(vx) > limit:
                    vx = limit * direction
            elif speed > limit:
                vx -= RELEASE_DECEL * direction
        if first_tick and jump_edge:
            start_jump()
    else:
        if direction:
            speed = abs(vx)
            if vx * direction >= 0:
                vx += (RUN_ACCEL if speed >= MAX_WALK else WALK_ACCEL) * direction
                if abs(vx) > air_max:
                    vx = air_max * direction
            elif speed >= MAX_WALK:
                vx += RUN_ACCEL * direction
            elif jump_speed >= 0x1D00:
                vx += RELEASE_DECEL * direction
            else:
                vx += WALK_ACCEL * direction

    if first_tick and dash_edge and ember and len(embers) < 2 and not crouch:
        embers.append([px + ((10 if face > 0 else -2) << 12), py + ((8 if big else 4) << 12),
                       EMBER_SPEED * face, 0x1000])

    # Gravity. Holding jump while rising makes the jump higher.
    vy += hold_gravity if (vy < 0 and jump) else fall_gravity
    if vy >= MAX_FALL:
        vy = FALL_CAP

    height = player_height()
    top = hitbox_top()

    # Horizontal movement and walls.
    px += vx
    x = px >> 12
    y = py >> 12
    if x + 3 < camera:
        x = camera - 3
        px = x << 12
        if vx < 0:
            vx = 0
    if x > width * 16 - 16:
        x = width * 16 - 16
        px = x << 12
    upper = y + top
    lower = y + height - 2
    middle = (upper + lower) >> 1
    if vx >= 0 and (solid_at(x + 12, upper) or solid_at(x + 12, middle) or solid_at(x + 12, lower)):
        x = ((x + 12) >> 4 << 4) - 13
        px = x << 12
        if vx > 0:
            vx = 0
    elif vx <= 0 and (solid_at(x + 3, upper) or solid_at(x + 3, middle) or solid_at(x + 3, lower)):
        x = (((x + 3) >> 4) + 1 << 4) - 3
        px = x << 12
        if vx < 0:
            vx = 0

    # Riding a lift.
    if lift_under is not None:
        px += lift_under[5] << 12
        x = px >> 12
        if vy >= 0 and lift_under[1] - 12 <= x <= lift_under[1] + 44:
            py = (lift_under[2] - height) << 12
            vy = 0
            on_ground = True
        else:
            lift_under = None

    # Vertical movement.
    previous_y = py >> 12
    py += vy
    y = py >> 12
    if vy >= 0:
        foot = y + height
        row = foot >> 4
        landed = False
        if (previous_y + height) <= (row << 4) + 1 or on_ground:
            if SOLID[tile_at((x + 4) >> 4, row)] == 1 or SOLID[tile_at((x + 11) >> 4, row)] == 1:
                if 0 <= row < ROWS:
                    y = (row << 4) - height
                    py = y << 12
                    vy = 0
                    landed = True
        if not landed and lift_under is None:
            for lift in lifts:
                if lift[1] - 12 <= x <= lift[1] + 44 and previous_y + height <= lift[2] + 2 <= foot + 2:
                    y = lift[2] - height
                    py = y << 12
                    vy = 0
                    landed = True
                    lift_under = lift
                    break
        if landed:
            if not on_ground:
                stomp_chain = 0
            on_ground = True
        elif lift_under is None:
            on_ground = False
    else:
        on_ground = False
        head = y + top
        row = head >> 4
        center = (x + 8) >> 4
        left_column = (x + 5) >> 4
        right_column = (x + 10) >> 4
        if 0 <= row < ROWS:
            hit = None
            if SOLID[tile_at(center, row)]:
                hit = center
            elif SOLID[tile_at(left_column, row)]:
                hit = left_column
            elif SOLID[tile_at(right_column, row)]:
                hit = right_column
            if hit is not None:
                bump_block(hit, row)
                y = ((row + 1) << 4) - top
                py = y << 12
                vy = 0

    # Walking animation speeds up with speed.
    speed = abs(vx)
    if on_ground and speed:
        anim_timer += 1
        if anim_timer >= (7 if speed < 0x1000 else 4 if speed < 0x2000 else 2):
            anim_timer = 0
            anim_step = (anim_step + 1) % 3
    elif on_ground:
        anim_step = 0

    # Gems in the map.
    for column in ((x + 4) >> 4, (x + 11) >> 4):
        for row in ((y + top) >> 4, (y + height - 2) >> 4):
            if tile_at(column, row) == T_GEM:
                set_tile(column, row, T_EMPTY)
                add_gem()

    if y > ROWS * 16 + 8:
        die(False)
    elif (x + 12) >= goal_column * 16 + 6 and state == PLAY:
        grab_pole()


def hurt():
    global big, ember, hurt_timer, state, state_timer
    if hurt_timer or comet_timer or state != PLAY:
        return
    if big:
        big = False
        ember = False
        hurt_timer = 150
        state = CHANGE
        state_timer = 40
    else:
        die(True)


def grow(to_ember):
    global big, ember, state, state_timer, py
    if to_ember:
        if not big:
            py -= 16 << 12
        big = True
        ember = True
        return
    if big:
        return
    big = True
    py -= 16 << 12
    state = CHANGE
    state_timer = 40


def die(jump):
    global state, state_timer, vy, big, ember, comet_timer
    if state == DYING:
        return
    state = DYING
    state_timer = 0 if jump else 60
    vy = 0
    if big:
        big = False
        ember = False
    comet_timer = 0


def grab_pole():
    global state, px, vx, vy, state_timer
    foot = (py >> 12) + player_height()
    points = 5000 if foot <= 64 else 2000 if foot <= 96 else 800 if foot <= 128 else 400 if foot <= 150 else 100
    add_score(points, goal_column * 16 + 12, foot - 12)
    state = POLE
    px = (goal_column * 16 - 5) << 12
    vx = vy = 0
    state_timer = 0


def bump_block(column, row):
    tile = tile_at(column, row)
    if tile in (T_USED, T_HARD, T_BUMPING) or tile == T_EMPTY:
        return
    if T_LOG_RIM_L <= tile <= T_LOG_R or T_CANOPY_L <= tile <= T_CANOPY_R or tile in (T_GRASS, T_DIRT):
        return
    wx = column * 16
    wy = row * 16
    # Whatever stands on the block is knocked off.
    for enemy in enemies:
        if enemy[5] in (WALK, SHELL, SHELL_MOVING):
            ex = enemy[1] >> 12
            ey = (enemy[2] >> 12) + 16
            if abs(ex - wx) < 14 and wy - 4 <= ey <= wy + 2:
                knock_out(enemy, 100)
    for item in items:
        if abs((item[1] >> 12) - wx) < 14 and abs((item[2] >> 12) + 16 - wy) < 4:
            item[4] = -0x3000
            item[3] = ITEM_SPEED if (item[1] >> 12) >= wx else -ITEM_SPEED
    if tile_at(column, row - 1) == T_GEM:
        set_tile(column, row - 1, T_EMPTY)
        gem_pop(wx, wy - 16)

    if tile == T_BRICK:
        if big:
            set_tile(column, row, T_EMPTY)
            add_score(50)
            for dx, dvx, dvy in ((0, -1, -5), (8, 1, -5), (0, -1, -3), (8, 1, -3)):
                effects.append([S_DEBRIS, wx + dx - 4, wy - 4, dvx, dvy, 60, 0])
            return
        start_bump(column, row, T_BRICK, S_BUMP_BRICK)
    elif tile == T_GEM_BLOCK:
        start_bump(column, row, T_USED, S_BUMP_GEM_BLOCK)
        gem_pop(wx, wy - 16)
    elif tile == T_BRICK_GEMS:
        hits = brick_gems.get(row * width + column, 0) + 1
        brick_gems[row * width + column] = hits
        start_bump(column, row, T_USED if hits >= 8 else T_BRICK_GEMS, S_BUMP_BRICK)
        gem_pop(wx, wy - 16)
    elif tile in (T_GEM_BLOCK_POWER, T_BRICK_POWER):
        start_bump(column, row, T_USED, S_BUMP_GEM_BLOCK if tile == T_GEM_BLOCK_POWER else S_BUMP_BRICK)
        items.append([FLOWER if big else SPROUT, wx << 12, wy << 12, ITEM_SPEED, 0, 16])
    elif tile == T_BRICK_COMET:
        start_bump(column, row, T_USED, S_BUMP_BRICK)
        items.append([COMET, wx << 12, wy << 12, ITEM_SPEED, 0, 16])
    elif tile == T_HIDDEN_LIFE:
        start_bump(column, row, T_USED, S_BUMP_USED)
        items.append([HEART, wx << 12, wy << 12, ITEM_SPEED, 0, 16])


def start_bump(column, row, result, cell):
    set_tile(column, row, T_BUMPING)
    bumps.append([column, row, 0, result, cell if result != T_USED or cell != S_BUMP_BRICK else S_BUMP_USED])


def gem_pop(x, y):
    add_gem()
    effects.append([S_GEM, x, y, 0, -6, 24, PAL_CHAR])


def knock_out(enemy, points):
    enemy[5] = FALLING
    enemy[4] = -0x3000
    enemy[3] = 0x800 if enemy[3] >= 0 else -0x800
    add_score(points, enemy[1] >> 12, enemy[2] >> 12)


# ---------------------------------------------------------------------------
# World

def spawn_objects():
    global next_object
    limit = camera + 320 + 24
    while next_object < object_count:
        base = next_object * 3
        column = objects[base + 1]
        if column * 16 > limit:
            break
        kind = objects[base]
        row = objects[base + 2]
        if kind in (NUTLET, BEETLE):
            speed = ENEMY_SPEED + lap * 0x200
            enemies.append([kind, column * 16 << 12, row * 16 << 12, -speed, 0, WALK, 0])
        elif kind in (LIFT_X, LIFT_Y):
            lifts.append([kind, column * 16, row * 16, column * 16 if kind == LIFT_X else row * 16, 1, 0, 0])
        next_object += 1


def enemies_tick():
    x0 = camera - 48
    player_x = px >> 12
    player_y = py >> 12
    height = player_height()
    top = hitbox_top()
    for enemy in enemies:
        kind, ex, ey, evx, evy, mode, timer = enemy
        if mode == FALLING:
            evy += 0x400
            ey += evy
            ex += evx
            if (ey >> 12) > ROWS * 16 + 16:
                enemy[0] = 0
        elif mode == FLAT:
            timer -= 1
            if timer <= 0:
                enemy[0] = 0
        else:
            if mode == SHELL:
                timer -= 1
                if timer <= 0:
                    mode = WALK
                    evx = -ENEMY_SPEED if player_x < (ex >> 12) else ENEMY_SPEED
            elif mode in (WALK, SHELL_MOVING):
                ex += evx
                x = ex >> 12
                y = ey >> 12
                front = x + 14 if evx > 0 else x + 1
                if solid_at(front, y + 6) or solid_at(front, y + 13) or front < 0:
                    evx = -evx
                    ex += evx
            evy += 0x700
            if evy > FALL_CAP:
                evy = FALL_CAP
            ey += evy
            x = ex >> 12
            y = ey >> 12
            row = (y + 16) >> 4
            if evy >= 0 and (SOLID[tile_at((x + 3) >> 4, row)] == 1 or SOLID[tile_at((x + 12) >> 4, row)] == 1):
                if 0 <= row < ROWS:
                    ey = ((row << 4) - 16) << 12
                    evy = 0
            if y > ROWS * 16 + 16 or x < x0:
                enemy[0] = 0
        enemy[1], enemy[2], enemy[3], enemy[4], enemy[5], enemy[6] = ex, ey, evx, evy, mode, timer

    # Enemies against each other.
    count = len(enemies)
    for i in range(count):
        a = enemies[i]
        if not a[0] or a[5] in (FALLING, FLAT):
            continue
        ax = a[1] >> 12
        ay = a[2] >> 12
        for j in range(i + 1, count):
            b = enemies[j]
            if not b[0] or b[5] in (FALLING, FLAT):
                continue
            if abs((b[1] >> 12) - ax) < 14 and abs((b[2] >> 12) - ay) < 14:
                if a[5] == SHELL_MOVING and b[5] != SHELL_MOVING:
                    knock_out(b, 500)
                elif b[5] == SHELL_MOVING and a[5] != SHELL_MOVING:
                    knock_out(a, 500)
                elif a[5] == SHELL_MOVING and b[5] == SHELL_MOVING:
                    knock_out(a, 500)
                    knock_out(b, 500)
                elif a[5] == WALK and b[5] == WALK:
                    if (a[3] > 0) == (ax < (b[1] >> 12)):
                        a[3] = -a[3]
                    if (b[3] > 0) == ((b[1] >> 12) < ax):
                        b[3] = -b[3]

    # Enemies against the player.
    if state == PLAY:
        left = player_x + 3
        right = player_x + 12
        upper = player_y + top
        lower = player_y + height - 1
        for enemy in enemies:
            if not enemy[0] or enemy[5] in (FALLING, FLAT):
                continue
            ex = enemy[1] >> 12
            ey = enemy[2] >> 12
            if right < ex + 2 or left > ex + 13 or lower < ey + 5 or upper > ey + 15:
                continue
            player_contact(enemy, ex, ey, lower)

    enemies[:] = [enemy for enemy in enemies if enemy[0]]


def player_contact(enemy, ex, ey, lower):
    global vy, stomp_chain, hurt_timer
    kind = enemy[0]
    mode = enemy[5]
    player_x = px >> 12
    if comet_timer:
        knock_out(enemy, 200)
        return
    if mode == SHELL and enemy[6] > 0:
        enemy[5] = SHELL_MOVING
        enemy[3] = SHELL_SPEED if player_x + 8 < ex + 8 else -SHELL_SPEED
        enemy[1] += enemy[3] * 3
        enemy[6] = 0
        add_score(400, ex, ey)
        if not hurt_timer:
            hurt_timer = 12
        return
    if vy > 0 and lower < ey + 11:
        points = STOMP_POINTS[stomp_chain] if stomp_chain < len(STOMP_POINTS) else -1
        stomp_chain += 1
        if points < 0:
            add_score(-1, ex, ey)
        else:
            add_score(points, ex, ey)
        if kind == NUTLET:
            enemy[5] = FLAT
            enemy[6] = 30
        else:
            enemy[5] = SHELL
            enemy[3] = 0
            enemy[6] = 360
        vy = -STOMP_BOUNCE
        return
    if hurt_timer:
        return
    hurt()


def items_tick():
    player_x = px >> 12
    player_y = py >> 12
    height = player_height()
    for item in items:
        kind, ix, iy, ivx, ivy, rising = item
        if rising:
            iy -= 1 << 12
            rising -= 1
        elif kind != FLOWER:
            ix += ivx
            x = ix >> 12
            y = iy >> 12
            front = x + 14 if ivx > 0 else x + 1
            if solid_at(front, y + 8):
                ivx = -ivx
                ix += ivx
            ivy += 0x500 if kind == COMET else 0x700
            if ivy > FALL_CAP:
                ivy = FALL_CAP
            iy += ivy
            y = iy >> 12
            row = (y + 16) >> 4
            if ivy >= 0 and 0 <= row < ROWS and (solid_at(x + 3, y + 16) or solid_at(x + 12, y + 16)):
                iy = ((row << 4) - 16) << 12
                ivy = -0x4000 if kind == COMET else 0
            if y > ROWS * 16 or x < camera - 32:
                item[0] = 0
        item[1], item[2], item[3], item[4], item[5] = ix, iy, ivx, ivy, rising
        if item[0] and not rising and state == PLAY:
            x = ix >> 12
            y = iy >> 12
            if abs(x - player_x) < 13 and player_y + 2 < y + 16 and y < player_y + height:
                collect(item)
    items[:] = [item for item in items if item[0]]


def collect(item):
    global comet_timer
    kind = item[0]
    x = item[1] >> 12
    y = item[2] >> 12
    item[0] = 0
    if kind == SPROUT:
        grow(False)
        add_score(1000, x, y)
    elif kind == FLOWER:
        grow(True)
        add_score(1000, x, y)
    elif kind == COMET:
        comet_timer = 600
        add_score(1000, x, y)
    elif kind == HEART:
        add_score(-1, x, y)


def embers_tick():
    for ball in embers:
        ball[0] += ball[2]
        ball[3] += 0x600
        if ball[3] > FALL_CAP:
            ball[3] = FALL_CAP
        ball[1] += ball[3]
        x = (ball[0] >> 12) + 8
        y = (ball[1] >> 12) + 8
        if ball[3] > 0 and solid_at(x, y + 3):
            ball[1] = (((y + 3) >> 4 << 4) - 12) << 12
            ball[3] = -0x2800
        if solid_at(x + (4 if ball[2] > 0 else -4), y) or x < camera - 8 or x > camera + 328 or y > ROWS * 16:
            effects.append([S_PUFF, x - 8, y - 8, 0, 0, 10, PAL_CHAR])
            ball[2] = 0
            continue
        for enemy in enemies:
            if enemy[5] in (FALLING, FLAT):
                continue
            if abs((enemy[1] >> 12) + 8 - x) < 10 and abs((enemy[2] >> 12) + 10 - y) < 10:
                knock_out(enemy, 200)
                effects.append([S_PUFF, x - 8, y - 8, 0, 0, 10, PAL_CHAR])
                ball[2] = 0
                break
    embers[:] = [ball for ball in embers if ball[2]]


def world_tick():
    for bump in bumps:
        bump[2] += 1
        if bump[2] >= len(BUMP_RISE):
            set_tile(bump[0], bump[1], bump[3])
    bumps[:] = [bump for bump in bumps if bump[2] < len(BUMP_RISE)]

    for effect in effects:
        effect[5] -= 1
        effect[1] += effect[3]
        effect[2] += effect[4]
        if effect[0] == S_DEBRIS:
            effect[4] += 1 if effect[5] % 2 else 0
        elif effect[0] == S_GEM and effect[5] < 14:
            effect[4] = 6 if effect[5] > 4 else 0
    effects[:] = [effect for effect in effects if effect[5] > 0]

    for lift in lifts:
        kind, x, y, origin, direction = lift[0], lift[1], lift[2], lift[3], lift[4]
        if frame_count % 2:
            lift[5] = lift[6] = 0
            continue
        if kind == LIFT_X:
            x += direction
            if x >= origin + 96 or x <= origin:
                direction = -direction
            lift[5], lift[6] = direction, 0
        else:
            y += direction
            if y >= origin + 96 or y <= origin:
                direction = -direction
            lift[5], lift[6] = 0, direction
        lift[1], lift[2], lift[4] = x, y, direction


def tick(first_tick):
    """One 60 Hz step of the whole stage."""
    global frame_count, state, state_timer, time_left, time_ticks, py, px, vy, vx
    global hidden, banner_y, camera, checkpoint_reached, score, face, on_ground
    frame_count += 1
    for index in range(4):
        entry = popups[index]
        if entry:
            entry[2] -= 1
            if entry[2] <= 0:
                popups[index] = None
            elif entry[2] % 2:
                entry[1] -= 1

    if state == PLAY:
        player_tick(first_tick)
        if state in (PLAY, CHANGE, POLE):
            spawn_objects()
            enemies_tick()
            items_tick()
            embers_tick()
            world_tick()
        time_ticks += 1
        if time_ticks >= 24:
            time_ticks = 0
            time_left -= 1
            if time_left <= 0:
                time_left = 0
                die(True)
        if not checkpoint_reached and checkpoint_column and (px >> 12) >= checkpoint_column * 16:
            checkpoint_reached = True
    elif state == CHANGE:
        state_timer -= 1
        if state_timer <= 0:
            state = PLAY
    elif state == DYING:
        state_timer += 1
        if state_timer == 30:
            vy = -0x4000
        elif state_timer > 30:
            vy += 0x280
            py += vy
        if state_timer > 180:
            return "dead"
    elif state == POLE:
        world_tick()
        state_timer += 1
        bottom = 10 * 16 - player_height()
        if (py >> 12) < bottom:
            py += 2 << 12
        if banner_y < 9 * 16:
            banner_y += 2
        elif (py >> 12) >= bottom:
            py = bottom << 12
            state = WALK_OUT
            state_timer = 0
            px = (goal_column * 16 + 9) << 12
            face = 1
    elif state == WALK_OUT:
        world_tick()
        state_timer += 1
        if state_timer > 20 and not hidden:
            vx = ITEM_SPEED
            px += vx
            vy += 0x700
            if vy > FALL_CAP:
                vy = FALL_CAP
            py += vy
            x = px >> 12
            foot = (py >> 12) + player_height()
            row = foot >> 4
            if solid_at(x + 8, foot):
                py = ((row << 4) - player_height()) << 12
                vy = 0
                on_ground = True
            global anim_timer, anim_step
            anim_timer += 1
            if anim_timer >= 6:
                anim_timer = 0
                anim_step = (anim_step + 1) % 3
            if x >= (goal_column + 7) * 16:
                hidden = True
                state = TALLY
                state_timer = 0
    elif state == TALLY:
        state_timer += 1
        if time_left > 0:
            step = min(time_left, 2)
            time_left -= step
            score += 50 * step
        elif state_timer > 150:
            return "clear"

    if state != DYING:
        x = px >> 12
        target = x - 136
        if target > camera:
            camera = min(target, width * 16 - 320)
    return None


# ---------------------------------------------------------------------------
# Drawing

hud_cache = [None, None]


def draw():
    sprite = cyd.tile_sprite
    cam = camera
    cyd.tile_frame(cam)

    for column, row, timer, _, cell in bumps:
        sprite(cell, column * 16 - cam, row * 16 + TOP - BUMP_RISE[timer])

    for kind, ix, iy, _, _, rising in items:
        x = (ix >> 12) - cam
        y = (iy >> 12) + TOP
        flags = PAL_CHAR | (BEHIND if rising else 0)
        if kind == SPROUT:
            sprite(S_SPROUT, x, y, flags)
        elif kind == FLOWER:
            sprite(S_EMBER_FLOWER, x, y, flags | (FLIP_X if frame_count & 8 else 0))
        elif kind == COMET:
            sprite(S_COMET, x, y, flags | (FLIP_Y if frame_count & 4 else 0))
        else:
            sprite(S_HEART, x, y, flags)

    walk_frame = (frame_count >> 4) & 1
    for kind, ex, ey, evx, _, mode, timer in enemies:
        x = (ex >> 12) - cam
        y = (ey >> 12) + TOP
        if kind == NUTLET:
            if mode == FLAT:
                sprite(S_NUT_FLAT, x, y)
            else:
                sprite(S_NUT_2 if walk_frame else S_NUT_1, x, y, FLIP_Y if mode == FALLING else 0)
        else:
            if mode in (SHELL, SHELL_MOVING):
                shake = 1 if mode == SHELL and timer < 90 and frame_count & 4 else 0
                sprite(S_SHELL, x + shake, y)
            elif mode == FALLING:
                sprite(S_SHELL, x, y, FLIP_Y)
            else:
                sprite(S_BEETLE_2 if walk_frame else S_BEETLE_1, x, y, FLIP_X if evx > 0 else 0)

    for lift in lifts:
        x = lift[1] - cam
        y = lift[2] + TOP
        sprite(S_LIFT, x, y, PAL_CHAR)
        sprite(S_LIFT, x + 16, y, PAL_CHAR)
        sprite(S_LIFT, x + 32, y, PAL_CHAR)

    draw_player(sprite, cam)

    for ball in embers:
        sprite(S_EMBER, (ball[0] >> 12) - cam, (ball[1] >> 12) + TOP,
               PAL_CHAR | (FLIP_X | FLIP_Y if frame_count & 4 else 0))
    for cell, x, y, _, _, timer, flags in effects:
        if cell == S_GEM:
            cell = (S_GEM, S_GEM_NARROW, S_GEM_EDGE, S_GEM_NARROW)[(timer >> 1) & 3]
        sprite(cell, x - cam, y + TOP, flags)

    if goal_column:
        sprite(S_BANNER, goal_column * 16 - 8 - cam, banner_y + TOP, PAL_CHAR)

    sprite(S_GEM, 62, 10)
    for index in range(4):
        entry = popups[index]
        if entry:
            cyd.tile_text(4 + index, entry[0] - cam, entry[1] + TOP, entry[3])
        else:
            cyd.tile_text(4 + index, 0, 0, "")

    line = "%06d   x%02d" % (score, gems)
    if line != hud_cache[0]:
        hud_cache[0] = line
        cyd.tile_text(2, 24, 18, line)
    line = " %s    %3d" % (stage_name, time_left)
    if line != hud_cache[1]:
        hud_cache[1] = line
        cyd.tile_text(3, 176, 18, line)
    return cyd.tile_present()


def draw_player(sprite, cam):
    if hidden:
        return
    if hurt_timer and state == PLAY and frame_count & 2:
        return
    x = (px >> 12) - cam
    y = (py >> 12) + TOP
    flags = PAL_CHAR
    if comet_timer:
        flags = (PAL_COMET, PAL_EMBER, PAL_CHAR)[(frame_count >> 1) % 3]
    elif ember:
        flags = PAL_EMBER
    if face < 0:
        flags |= FLIP_X
    if state == DYING:
        sprite(S_PIP_HURT, x, y, flags & ~FLIP_X)
        return
    growing = state == CHANGE and (state_timer >> 3) & 1
    show_big = big != bool(growing)
    if state == POLE:
        small_cell, bottom_cell = S_PIP_JUMP, S_BIG_JUMP
    elif not on_ground and state == PLAY:
        small_cell, bottom_cell = S_PIP_JUMP, S_BIG_JUMP
    elif skid:
        small_cell, bottom_cell = S_PIP_SKID, S_BIG_WALK1 + 1
    elif abs(vx) or state == WALK_OUT:
        small_cell, bottom_cell = S_PIP_WALK1 + anim_step, S_BIG_WALK1 + anim_step
    else:
        small_cell, bottom_cell = S_PIP_STAND, S_BIG_STAND
    if big and crouch:
        sprite(S_PIP_STAND, x, y + 16, flags)
    elif show_big:
        base = y if big else y - 16
        sprite(S_BIG_HEAD, x, base, flags)
        sprite(bottom_cell, x, base + 16, flags)
    else:
        sprite(small_cell, x, y + (16 if big else 0), flags)


# ---------------------------------------------------------------------------
# Screens

blank_map = bytearray(20 * ROWS)


def show_card(lines, theme, frames, figure=False, wait_key=False):
    """A dark screen with centred text; returns the key that ended it."""
    cyd.tile_begin(dark_palette(theme), tile_cells, sprite_cells, blank_map, 20, ROWS, TOP)
    for slot in range(8):
        cyd.tile_text(slot, 0, 0, "")
    for slot, (text, y, scale) in enumerate(lines):
        cyd.tile_text(slot, 160 - len(text) * 3 * scale, y, text, 1, scale)
    shown = 0
    while True:
        if not cyd.update():
            raise Quit()
        key = read_input()
        cyd.tile_frame(0)
        if figure:
            cyd.tile_sprite(S_PIP_STAND, 104, 112, PAL_CHAR)
        if not cyd.tile_present():
            raise Quit()
        shown += 1
        if wait_key and (key == "ENTER" or jump_edge):
            return key
        if not wait_key and shown >= frames:
            return None


def title_screen():
    global stage_number
    stage_number = 1
    load_stage(1)
    cyd.tile_begin(palettes[0], tile_cells, sprite_cells, tilemap, width, ROWS, TOP)
    for slot in range(8):
        cyd.tile_text(slot, 0, 0, "")
    cyd.tile_text(0, 80, 60, "PIP QUEST", 2, 3)
    cyd.tile_text(1, 81, 61, "PIP QUEST", 1, 3)
    cyd.tile_text(2, 118, 108, "PUSH ENTER", 1, 1)
    cyd.tile_text(3, 64, 132, "ARROWS MOVE  Z JUMP  X DASH", 2, 1)
    cyd.tile_text(4, 112, 22, "TOP %06d" % top_score, 1, 1)
    blink = 0
    while True:
        if not cyd.update():
            raise Quit()
        key = read_input()
        blink += 1
        cyd.tile_text(2, 118, 108, "PUSH ENTER" if (blink >> 4) & 1 else "", 1, 1)
        cyd.tile_frame(0)
        cyd.tile_sprite(S_PIP_STAND, 48, 10 * 16 + TOP, PAL_CHAR)
        if not cyd.tile_present():
            raise Quit()
        if key == "ENTER" or jump_edge:
            return


def play_stage():
    """Runs one attempt at the current stage: 'clear', 'dead' or raises Quit."""
    global hud_cache
    hud_cache = [None, None]
    start_stage()
    last = time.ticks_ms()
    backlog = 0
    paused = False
    while True:
        if not cyd.update():
            raise Quit()
        key = read_input()
        if key in ("ENTER", "p"):
            paused = not paused
            cyd.tile_text(4, 142, 100, "PAUSE" if paused else "")
        now = time.ticks_ms()
        elapsed = time.ticks_diff(now, last)
        last = now
        if paused:
            if not draw():
                raise Quit()
            continue
        # Fixed 60 Hz steps; at most four a frame so a stall does not jump.
        backlog = min(backlog + elapsed * 3, 200)
        first = True
        while backlog >= 50:
            backlog -= 50
            result = tick(first)
            first = False
            if result:
                return result
        if not draw():
            raise Quit()


def main(gfx_data=None, map_buffer=None):
    """main.py passes the big buffers, allocated before this module was loaded."""
    global score, gems, lives, stage_number, lap, checkpoint_reached, big, ember, top_score, tilemap
    cyd.no_time_limit()
    tilemap = map_buffer if map_buffer is not None else bytearray(MAP_CAPACITY)
    load_gfx(gfx_data)
    try:
        saved = cyd.storage_get("top")
        top_score = int(saved) if saved else 0
    except Exception:
        top_score = 0
    try:
        while True:
            title_screen()
            score = gems = 0
            lives = 3
            stage_number = 1
            lap = 0
            checkpoint_reached = False
            big = ember = False
            while lives > 0:
                name = "%d-%d" % (lap + 1, stage_number)
                show_card([("WORLD " + name, 80, 2), ("x  %d" % lives, 116, 2)], 0, 90, figure=True)
                result = play_stage()
                if result == "clear":
                    checkpoint_reached = False
                    stage_number += 1
                    if stage_number > 3:
                        stage_number = 1
                        lap += 1
                        show_card([("WELL DONE, PIP!", 80, 2), ("THE PATH GOES ON...", 120, 1)], 0, 180)
                else:
                    lives -= 1
                    big = ember = False
            if score > top_score:
                top_score = score
                cyd.storage_put("top", str(top_score))
            show_card([("GAME OVER", 100, 2)], 0, 150)
    except Quit:
        pass
