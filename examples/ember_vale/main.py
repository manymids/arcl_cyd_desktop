# EMBER VALE. The game is game.mpy, built from examples/ember_vale_src.
# The data files are read before the game's code is loaded, while the heap is
# still in one piece.
import gc
import sys

gc.collect()
_dir = __file__.rsplit("/", 1)[0]
with open(_dir + "/gfx.bin", "rb") as _source:
    _gfx = _source.read()
with open(_dir + "/world.bin", "rb") as _source:
    _world = _source.read()
sys.path.insert(0, _dir)
gc.collect()
import game

game.main(_gfx, _world)
