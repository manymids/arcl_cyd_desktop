# NEON PULSE

An animated MicroPython dashboard for CYD Desktop. It uses only the public
`cyd` API and demonstrates retained dirty updates, four touch callbacks, and
per-app persistent storage.

- `MODE` changes the spectrum algorithm.
- `BOOST` changes the animation rate.
- `FX +` advances the simulated effect bank.
- `RESET` resets the frame and uptime counters.
- Long-press Home or tap the taskbar Home control to exit.

Install it on the connected board with:

```powershell
npm run install:neon
```

The installer uploads `/sd/apps/neonpulse/main.py`, creates a `NEON PULSE`
Home shortcut if a shortcut slot is available, and launches the demo.

