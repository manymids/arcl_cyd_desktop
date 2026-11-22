# BOUNCE LAB

Tap to drop balls into a box of pegs. A small demo of the API added at levels
7 to 9, used together:

- `cyd.phys_*`: the balls, pegs, floor and walls are simulated natively
- `cyd.load_image` and `cyd.game_bitmap`: every ball is `ball.rgb`, scaled to
  its radius
- `cyd.overlay_*`: the status bar and the hint
- `cyd.touch_edge`: one ball per tap; `cyd.key_state`: Space shakes the box and
  C clears it on a Bluetooth keyboard
- `cyd.features`: the app says what is missing on older firmware

Up to 40 balls; after that the oldest is reused. `ball.rgb` (16×16 RGB565) is
drawn by `examples/bounce_lab_src/make_ball.py`:

```bash
python3 examples/bounce_lab_src/make_ball.py
node bridge/deploy.mjs examples/bounce_lab --launch
```
