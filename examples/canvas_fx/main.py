"""Neon animation built with CYD Desktop's retained lightweight Canvas API."""

import math
import time
import cyd


PALETTES = (
    (cyd.rgb(3, 10, 27), cyd.CYAN, cyd.MAGENTA, cyd.YELLOW),
    (cyd.rgb(13, 3, 28), cyd.rgb(176, 40, 255), cyd.ORANGE, cyd.CYAN),
    (cyd.rgb(0, 21, 18), cyd.rgb(0, 255, 142), cyd.YELLOW, cyd.rgb(30, 130, 255)),
)
MODES = ("AURORA", "LASER", "COMET")

frame = 0
mode = 0
palette = 0
frozen = False


def next_mode():
    global mode
    mode = (mode + 1) % len(MODES)


def next_palette():
    global palette
    palette = (palette + 1) % len(PALETTES)


def toggle_freeze():
    global frozen
    frozen = not frozen


def wave_y(index, phase):
    if mode == 1:
        tooth = (index * 13 + phase * 4) % 44
        return 94 + abs(tooth - 22) - 11
    if mode == 2:
        pulse = (index * 17 - phase * 5) % 150
        return 105 - max(0, 24 - abs(pulse - 50))
    return 94 + int(13 * math.sin(index * 0.82 + phase * 0.16))


def draw():
    dark, primary, secondary, hot = PALETTES[palette]
    phase = frame % 360

    cyd.clear()
    cyd.title("CANVAS FX")

    # One retained background and a perspective-like neon grid.
    cyd.fill_rect(16, 68, 288, 98, dark)
    for x in range(32, 289, 32):
        cyd.line(x, 70, x, 164, cyd.rgb(8, 38, 58))
    for y in range(72, 165, 18):
        cyd.line(18, y, 302, y, cyd.rgb(14, 30, 58))
    cyd.rect(16, 68, 288, 98, primary, 1)

    # Four fixed corner flares add depth without creating animation dirties.
    cyd.fill_rect(18, 70, 12, 2, hot)
    cyd.fill_rect(290, 70, 12, 2, hot)
    cyd.fill_rect(18, 162, 12, 2, secondary)
    cyd.fill_rect(290, 162, 12, 2, secondary)

    # Twelve neon dashes form an animated oscilloscope trace. Short filled
    # spans are much cheaper on this LCD than hundreds of isolated pixels.
    for index in range(12):
        x = 20 + index * 24
        y = wave_y(index, phase)
        cyd.fill_rect(x, y, 13, 3, hot if index % 4 == 0 else primary)

    # Ten spectrum bars keep their slots, so only changed old/new bounds dirty.
    for index in range(10):
        energy = 12 + int(31 * (0.5 + 0.5 * math.sin(
            phase * 0.13 + index * 0.91 + mode)))
        color = secondary if index % 3 else hot
        cyd.fill_rect(23 + index * 27, 161 - energy, 14, energy, color)

    # A glowing orbital badge completes the 48-command scene.
    cyd.circle(263, 94, 20, secondary, 2)
    cyd.fill_circle(263, 94, 3, hot)
    for offset in (0.0, 2.1, 4.2):
        angle = phase * 0.055 + offset
        cyd.fill_circle(263 + int(20 * math.cos(angle)),
                        94 + int(20 * math.sin(angle)), 2, primary)

    cyd.button("mode", 18, 170, 88, 21, MODES[mode], next_mode)
    cyd.button("color", 116, 170, 88, 21, "COLOR %d" % (palette + 1), next_palette)
    cyd.button("freeze", 214, 170, 88, 21,
               "RESUME" if frozen else "FREEZE", toggle_freeze)
    cyd.log("48 SHAPES  %s  %s" % (MODES[mode], "PAUSED" if frozen else "LIVE"))


draw()
while cyd.update():
    if not frozen:
        frame += 1
    draw()
    time.sleep_ms(85)
