# Pixel Storm

A MicroPython-authored retro bullet-hell shooter for CYD Desktop's native batch
Game API. Drag anywhere in the playfield to move. The ship fires automatically;
the taskbar Home button and long press remain available.

The Python program owns movement, spawning, collision, score, lives, and stage
timing. Native code draws the submitted batch of at most 128 procedural sprites
into the shared 320x16 RGB565 transition tile.
