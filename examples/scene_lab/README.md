# SCENE LAB

A ring of cubes around a spinning gem over a ground plane, with glowing sprites
orbiting in front: a demo of the API added at levels 10 to 12, used together.

- `cyd.n3d_pack_model`, `cyd.n3d_load`, `cyd.n3d_camera`, `cyd.n3d_present`:
  Z-buffered 3D with flat lighting and fog, the camera circling the scene
- `cyd.sprite_create` and the `sprites` argument of `cyd.n3d_present`:
  additive glow sprites, scaled and turning, drawn over the 3D
- `cyd.overlay_*`: the status line with the polygon count and frame rate

Tap the screen (or W on a Bluetooth keyboard) to toggle wire frames, F to
toggle fog, Space to pause the spin. Everything is built at start-up, so the
app has no asset files.

```bash
node bridge/deploy.mjs examples/scene_lab --launch
```
