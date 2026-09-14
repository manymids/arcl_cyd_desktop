# PIP QUEST. The game is game.mpy, built from examples/pip_quest_src.
#
# With Bluetooth on, the heap is small: take the two big buffers while it is
# still in one piece, before the game's code is loaded.
import gc
import sys

gc.collect()
_dir = __file__.rsplit("/", 1)[0]
with open(_dir + "/gfx.bin", "rb") as _source:
    _gfx = _source.read()
_map = bytearray(2600)
sys.path.insert(0, _dir)
gc.collect()
import game

game.main(_gfx, _map)
