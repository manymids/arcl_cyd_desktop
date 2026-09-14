# CANVAS FX

A 48-command RGB565 Canvas demo for CYD Desktop. It combines a neon grid,
animated oscilloscope, spectrum bars, orbiting particles, and three touch
controls without allocating a framebuffer.

The public API used by the demo is `rgb`, `pixel`, `line`, `rect`,
`fill_rect`, `circle`, and `fill_circle`. Calling `clear()` starts the next
retained frame; `update()` compares it with the last displayed frame and sends
only old/new primitive bounds to the LCD in four-pixel scan bands.

Install and launch it (set `CYD_DESKTOP_PORT` if more than one CH340 board is attached):

```powershell
npm run install:canvas
```
