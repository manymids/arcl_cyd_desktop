"""Draw ball.rgb for examples/bounce_lab: a 16x16 shaded sphere.

Headerless little-endian RGB565, colour 0 transparent, as cyd.load_image()
expects. Run from the repository root:

    python3 examples/bounce_lab_src/make_ball.py
"""

import os
import struct

SIZE = 16
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bounce_lab", "ball.rgb")


def rgb565(red, green, blue):
    value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
    return value or 0x0821  # never emit the transparent colour inside the ball


def main():
    pixels = bytearray()
    centre = (SIZE - 1) / 2
    radius = SIZE / 2
    for y in range(SIZE):
        for x in range(SIZE):
            dx, dy = x - centre, y - centre
            if dx * dx + dy * dy > radius * radius:
                pixels += struct.pack("<H", 0)
                continue
            # Light from the upper left: bright highlight fading to a dark rim.
            distance = ((x - centre + 3.5) ** 2 + (y - centre + 3.5) ** 2) ** 0.5 / (SIZE * 0.85)
            light = max(0.0, 1.0 - distance)
            rim = ((dx * dx + dy * dy) ** 0.5) / radius
            shade = min(1.0, 0.25 + 0.95 * light - 0.2 * rim)
            red = int(40 + 215 * shade)
            green = int(90 + 160 * shade ** 1.5)
            blue = int(30 + 70 * shade ** 3)
            pixels += struct.pack("<H", rgb565(min(255, red), min(255, green), min(255, blue)))
    with open(OUT, "wb") as target:
        target.write(pixels)
    print("wrote", os.path.normpath(OUT), len(pixels), "bytes")


if __name__ == "__main__":
    main()
