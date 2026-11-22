# SCENE LAB: a ring of cubes around a spinning gem, with glowing sprites
# orbiting in front. Shows the API added at levels 10-12 together:
#   n3d_pack_model / n3d_load / n3d_camera / n3d_present   Z-buffered 3D, lit, fogged
#   sprite_create + the sprites argument of n3d_present     additive, scaled, turning
#   overlay_*                                               the status line
# Tap (or W on a keyboard) toggles wire frames; F toggles fog; Space pauses the spin.
import math
import struct
import time

import cyd

missing = [name for name in ("n3d", "sprite", "overlay") if name not in cyd.features()]
if missing:
    cyd.title("SCENE LAB")
    cyd.text(8, 40, "Needs API level 12: no " + ", ".join(missing))
    while cyd.update():
        time.sleep_ms(100)
    raise SystemExit


def outward(verts, faces):
    """Wind every face of a convex model centred on the origin so it faces out."""
    fixed = []
    for face in faces:
        corners, color = list(face[:-1]), face[-1]
        a, b, c = verts[corners[0]], verts[corners[1]], verts[corners[2]]
        u = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        w = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        normal = (u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0])
        centre = [sum(verts[i][k] for i in corners) for k in range(3)]
        if sum(normal[k] * centre[k] for k in range(3)) < 0:
            corners.reverse()
        fixed.append(tuple(corners) + (color,))
    return fixed


CUBE = [(-24, -24, -24), (24, -24, -24), (24, 24, -24), (-24, 24, -24),
        (-24, -24, 24), (24, -24, 24), (24, 24, 24), (-24, 24, 24)]
CUBE_FACES = [(0, 1, 2, 3, 0xF800), (4, 5, 6, 7, 0xF800), (0, 3, 7, 4, 0xFD20),
              (1, 2, 6, 5, 0xFD20), (3, 2, 6, 7, 0xFFE0), (0, 1, 5, 4, 0xFFE0)]
GEM = [(0, -60, 0), (0, 60, 0), (40, 0, 0), (-40, 0, 0), (0, 0, 40), (0, 0, -40)]
GEM_FACES = [(0, 2, 4, 0x07FF), (0, 4, 3, 0x05BF), (0, 3, 5, 0x07FF), (0, 5, 2, 0x05BF),
             (1, 4, 2, 0x04DF), (1, 3, 4, 0x07FF), (1, 5, 3, 0x04DF), (1, 2, 5, 0x07FF)]
GROUND = [(-260, -90, -260), (260, -90, -260), (260, -90, 260), (-260, -90, 260)]  # +y is up

cyd.n3d_load(0, cyd.n3d_pack_model(CUBE, outward(CUBE, CUBE_FACES), flags=0))
cyd.n3d_load(1, cyd.n3d_pack_model(GEM, outward(GEM, GEM_FACES), flags=0))
# The ground is seen from above only, so it is two-sided rather than wound.
cyd.n3d_load(2, cyd.n3d_pack_model(GROUND, [(0, 1, 2, 3, 0x2945)], flags=1))


def glow(size):
    """A round additive glow: bright centre fading to transparent."""
    pixels = bytearray(size * size * 2)
    centre = (size - 1) / 2
    for y in range(size):
        for x in range(size):
            d = math.sqrt((x - centre) ** 2 + (y - centre) ** 2) / (size / 2)
            if d >= 1:
                continue
            k = (1 - d) ** 1.5
            color = cyd.rgb(int(120 + 135 * k), int(160 + 95 * k), 255) if k > 0.6 else cyd.rgb(
                int(40 * k / 0.6), int(120 * k / 0.6), int(255 * k / 0.6))
            color = color or 0x0001
            pixels[(y * size + x) * 2] = color & 0xFF
            pixels[(y * size + x) * 2 + 1] = color >> 8
    return pixels


SPARK = cyd.sprite_create(16, 16, glow(16))
SPARKS = 18
sprites = bytearray(cyd.SPRITE_RECORD * SPARKS)

wire = False
fog_on = True
spinning = True
spin = 0.0
frames = 0
fps = 0
counted = 0
last_count = time.ticks_ms()
last = last_count
polys = 0

while cyd.update():
    now = time.ticks_ms()
    dt = min(0.1, time.ticks_diff(now, last) / 1000)
    last = now
    if cyd.touch_edge() is not None or cyd.key_state("w") & cyd.KEY_PRESSED:
        wire = not wire
    if cyd.key_state("f") & cyd.KEY_PRESSED:
        fog_on = not fog_on
    if cyd.key_state(" ") & cyd.KEY_PRESSED:
        spinning = not spinning
    if spinning:
        spin += dt

    orbit = spin * 0.35
    cyd.n3d_camera(eye=(math.sin(orbit) * 330, 150, -math.cos(orbit) * 330), at=(0, 0, 0))
    flags = cyd.N3D_ENABLE | (cyd.N3D_WIRE if wire else 0)
    angle = int(spin * 9000) & 0xFFFF
    items = [(2, (0, 0, 0), (0, 0, 0), cyd.N3D_ENABLE)]
    for index in range(6):
        a = index * math.pi / 3 + spin * 0.6
        items.append((0, (math.cos(a) * 150, 20 * math.sin(spin * 2 + index), math.sin(a) * 150),
                      (angle + index * 9000, angle // 2, index * 5000), flags))
    items.append((1, (0, -20 + 12 * math.sin(spin * 1.5), 0), (0, (angle * 2) & 0xFFFF, 0), flags))

    for index in range(SPARKS):
        a = index * 2 * math.pi / SPARKS - spin * 0.9
        radius = 92 + 14 * math.sin(spin * 2 + index)
        scale = 200 + int(120 * (0.5 + 0.5 * math.sin(spin * 3 + index * 0.7)))
        struct.pack_into("<HhhHHHhhHBBH", sprites, index * cyd.SPRITE_RECORD, SPARK,
                         160 + int(math.cos(a) * radius * 1.4), 112 + int(math.sin(a) * radius * 0.55),
                         scale, scale, (angle * 3 + index * 3000) & 0xFFFF, 8, 8, 0, 255, cyd.BLEND_ADD, 0)

    cyd.overlay_rect(0, 0, 320, 12, cyd.BLACK)
    cyd.overlay_text(4, 2, "SCENE LAB", cyd.CYAN, shadow=0)
    cyd.overlay_text(316, 2, "%d POLY  %d FPS" % (polys, fps), cyd.GREEN, shadow=0, align=cyd.ALIGN_RIGHT)
    cyd.overlay_text(160, 228, "TAP: WIRE  F: FOG  SPACE: PAUSE", cyd.WHITE, align=cyd.ALIGN_CENTER)
    polys = cyd.n3d_present(items, sprites, clear=0x0841,
                            fog=(0x0841, 300 * 65536, 650 * 65536) if fog_on else (0, 0, 0),
                            light=(-6000, -12000, -8000, 90))
    if polys < 0:
        break

    frames += 1
    counted += 1
    if time.ticks_diff(now, last_count) >= 1000:
        fps = counted * 1000 // time.ticks_diff(now, last_count)
        counted = 0
        last_count = now
