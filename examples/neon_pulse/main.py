"""Animated CYD Desktop demo using only the public MicroPython API."""

import time
import cyd


MODES = ("PULSE", "WAVE", "MATRIX")
MARQUEE = " CYD DESKTOP // MICROPYTHON // DIRTY RECTANGLES // "
SPARKS = (".:-=+*#%@#*+=-:", ":=+#@#+=:.:-+*", "@#*=-:..:-=*#@")

mode = 0
boost = False
color = 0
frame = 0
started = time.ticks_ms()

try:
    launches = int(cyd.storage_get("launches", "0")) + 1
except (TypeError, ValueError):
    launches = 1
cyd.storage_put("launches", launches)


def meter(value, width=18):
    value = max(0, min(100, int(value)))
    filled = value * width // 100
    return "[" + "#" * filled + "." * (width - filled) + "]"


def cycle_mode():
    global mode
    mode = (mode + 1) % len(MODES)


def toggle_boost():
    global boost
    boost = not boost


def cycle_color():
    global color
    color = (color + 1) % 6


def reset_counters():
    global frame, started
    frame = 0
    started = time.ticks_ms()


def draw():
    phase = frame % 100
    cpu = (phase * 7 + 23 + mode * 11) % 101
    net = (phase * 11 + 47 + color * 5) % 101
    glow = (phase * 17 + (35 if boost else 0)) % 101
    scroll = frame % len(MARQUEE)
    banner = (MARQUEE + MARQUEE)[scroll:scroll + 43]
    spark = SPARKS[mode]
    offset = frame % len(spark)
    spectrum = (spark + spark)[offset:offset + 22]
    uptime = time.ticks_diff(time.ticks_ms(), started) // 1000

    # Rebuild one retained frame. The native layer compares it with the last
    # frame and redraws only changed element bounds.
    cyd.clear()
    cyd.title("NEON PULSE")
    cyd.text(20, 78,  "// LIVE CYBERDECK   RUN %03d" % launches)
    cyd.text(20, 90,  "MODE %-6s  BOOST %-3s  FX %d" %
             (MODES[mode], "ON" if boost else "OFF", color + 1))
    cyd.text(20, 102, "CPU  %s %3d%%" % (meter(cpu), cpu))
    cyd.text(20, 114, "NET  %s %3d%%" % (meter(net), net))
    cyd.text(20, 126, "GLOW %s %3d%%" % (meter(glow), glow))
    cyd.text(20, 138, "SPECTRUM  <%s>" % spectrum)
    cyd.text(20, 150, banner)
    cyd.button("mode", 18, 163, 67, 27, "MODE", cycle_mode)
    cyd.button("boost", 90, 163, 67, 27,
               "BOOST!" if boost else "BOOST", toggle_boost)
    cyd.button("color", 162, 163, 67, 27, "FX +", cycle_color)
    cyd.button("reset", 234, 163, 67, 27, "RESET", reset_counters)
    cyd.log("FRAME %06d  UP %04ds  TAP THE CONTROLS" % (frame, uptime))


draw()
while cyd.update():
    now = time.ticks_ms()
    frame += 1
    draw()
    time.sleep_ms(70 if boost else 125)

