# EMBER VALE

A small top-down RPG demo on the tile map view (`cyd.tile_*`, API level 6):
walk the valley with the on-screen pad, talk to villagers (A), open chests,
show the status window (B) and fight monsters in the fields. Characters,
monsters, map and text are original. No hardware keyboard is used.

Built from `examples/ember_vale_src`:

```bash
python3 examples/ember_vale_src/build.py          # gfx.bin, world.bin, game.mpy
python3 examples/ember_vale_src/sim.py 3000       # run on the PC with a stand-in cyd
node bridge/deploy.mjs examples/ember_vale --pin --launch
```
