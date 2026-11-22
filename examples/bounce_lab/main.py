# BOUNCE LAB: tap to drop balls into a box of pegs.
#
# Shows the API added at levels 7-9 working together:
#   phys_*            the balls, pegs, floor and walls are simulated natively
#   load_image /      every ball is ball.rgb, scaled to its radius and
#   game_bitmap       mirrored on alternate balls
#   overlay_*         the status bar and the hint line
#   touch_edge        one ball per tap, at the tap
#   key_state         Space shakes the box, C clears it (Bluetooth keyboard)
#   features          refuses to start on firmware without these
import struct
import time

import cyd

FLOOR_Y = 206
LEFT_X = 6
RIGHT_X = 314
FIRST_BALL = 8           # slots 0..7 hold the floor, walls and pegs
PEGS = ((80, 120), (160, 150), (240, 120), (120, 90), (200, 90))
PEG_HALF = 7
GRAVITY = 420.0

missing = [name for name in ("phys", "image", "overlay") if name not in cyd.features()]
if missing:
    cyd.title("BOUNCE LAB")
    cyd.text(8, 40, "Needs API level 9: no " + ", ".join(missing))
    while cyd.update():
        time.sleep_ms(100)
    raise SystemExit

ball = cyd.load_image("ball.rgb", 16, 16)
state = bytearray(cyd.PHYS_CAP * cyd.PHYS_STRIDE)
radii = {}
next_slot = FIRST_BALL
seed = 12345


def random(limit):
    """A small LCG; good enough for spawn jitter."""
    global seed
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
    return seed % limit


def build_box():
    radii.clear()
    cyd.phys_reset(2, (0.0, GRAVITY, 0.0))
    cyd.phys_body(0, cyd.PHYS_PLANE, (0, FLOOR_Y, 0), (0, -1, 0), mass=0, restitution=0.5)
    cyd.phys_body(1, cyd.PHYS_PLANE, (LEFT_X, 0, 0), (1, 0, 0), mass=0, restitution=0.8)
    cyd.phys_body(2, cyd.PHYS_PLANE, (RIGHT_X, 0, 0), (-1, 0, 0), mass=0, restitution=0.8)
    for index, (x, y) in enumerate(PEGS):
        cyd.phys_body(3 + index, cyd.PHYS_BOX, (x, y, 0), (PEG_HALF, PEG_HALF, PEG_HALF),
                      mass=0, restitution=0.7, friction=0.2)


def drop(x, y):
    """Put a ball at (x, y), reusing the oldest slot once all are taken."""
    global next_slot
    radius = 5 + random(6)
    x = min(RIGHT_X - radius - 1, max(LEFT_X + radius + 1, x))
    y = min(FLOOR_Y - radius - 1, max(24 + radius, y))
    cyd.phys_body(next_slot, cyd.PHYS_SPHERE, (x, y, 0), radius,
                  mass=radius * radius / 50.0, restitution=0.55, friction=0.3)
    cyd.phys_velocity(next_slot, (random(121) - 60, -random(80), 0))
    radii[next_slot] = radius
    next_slot = FIRST_BALL + (next_slot - FIRST_BALL + 1) % (cyd.PHYS_CAP - FIRST_BALL)


def shake():
    for slot in radii:
        cyd.phys_velocity(slot, (random(241) - 120, -260 - random(120), 0))


build_box()
for index in range(6):
    drop(40 + index * 48, 30 + (index % 2) * 20)

cyd.game_begin(cyd.NAVY, cyd.BLACK, cyd.CYAN)
frame = 0
fps = 0
fps_frames = 0
fps_start = time.ticks_ms()
last = time.ticks_ms()

while cyd.update():
    now = time.ticks_ms()
    dt = min(0.05, time.ticks_diff(now, last) / 1000)
    last = now

    tap = cyd.touch_edge()
    if tap is not None and tap[1] < FLOOR_Y:
        drop(tap[0], tap[1])
    if cyd.key_state(" ") & cyd.KEY_PRESSED:
        shake()
    if cyd.key_state("c") & cyd.KEY_PRESSED:
        build_box()

    cyd.phys_step(dt)
    cyd.phys_state(state)

    cyd.game_frame(frame, len(radii), 0, 0)
    for x, y in PEGS:
        cyd.game_quad(x - PEG_HALF, y - PEG_HALF, x + PEG_HALF, y - PEG_HALF,
                      x + PEG_HALF, y + PEG_HALF, x - PEG_HALF, y + PEG_HALF, cyd.ORANGE)
    cyd.game_quad(LEFT_X, FLOOR_Y, RIGHT_X, FLOOR_Y, RIGHT_X, FLOOR_Y + 5, LEFT_X, FLOOR_Y + 5, cyd.CYAN)
    for slot, radius in radii.items():
        x, y = struct.unpack_from("<2f", state, slot * cyd.PHYS_STRIDE)
        cyd.game_bitmap(ball, int(x), int(y), scale=radius * 2 * 100 // 16, flags=slot & 1)

    cyd.overlay_rect(0, 18, 320, 11, cyd.BLACK)
    cyd.overlay_text(4, 20, "BALLS %d" % len(radii), cyd.YELLOW, shadow=0)
    cyd.overlay_text(316, 20, "%d FPS" % fps, cyd.GREEN, shadow=0, align=cyd.ALIGN_RIGHT)
    if frame < 150:
        cyd.overlay_text(160, 60, "TAP TO DROP", cyd.WHITE, 2, align=cyd.ALIGN_CENTER)
        cyd.overlay_text(160, 80, "SPACE SHAKE  C CLEAR", cyd.WHITE, align=cyd.ALIGN_CENTER)

    frame += 1
    fps_frames += 1
    if time.ticks_diff(now, fps_start) >= 1000:
        fps = fps_frames * 1000 // time.ticks_diff(now, fps_start)
        fps_frames = 0
        fps_start = now
