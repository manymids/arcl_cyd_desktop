# PIP QUEST

A side-scrolling platformer in MicroPython on the firmware's tile map view
(`cyd.tile_*`, API level 5). Characters, art and stages are original.

Keys (Bluetooth keyboard): arrows or A/D move, Z or Space jumps (hold for
higher), X or Shift dashes and throws embers with the lantern, Down crouches,
Enter starts and pauses.

The files here are built from `examples/pip_quest_src`:

```bash
python3 examples/pip_quest_src/build.py            # gfx.bin, stage*.bin, game.mpy
python3 examples/pip_quest_src/sim.py 3000         # run on the PC with a stand-in cyd
node bridge/deploy.mjs examples/pip_quest --pin --launch
```
