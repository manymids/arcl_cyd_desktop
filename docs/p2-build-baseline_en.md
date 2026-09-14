# P2: ESP-IDF / MicroPython build baseline

English | [日本語](p2-build-baseline_ja.md)

Date: 2026-09-07

## Result

A custom MicroPython board for CYD Desktop Sandbox was successfully built for ESP32.

| Item | Value |
| --- | --- |
| MicroPython | v1.29.0 |
| ESP-IDF | v5.5.2 |
| Target | ESP32-D0WD-V3 / 4 MiB flash / no PSRAM |
| Combined firmware | 759 KiB |
| Board overlay | `firmware/cyd_desktop_board` |
| Output | `~/cyd-desktop-vendor/micropython/ports/esp32/build-cyd_desktop_board/firmware.bin` (WSL) |

## Build environment

With ESP-IDF CMake/Ninja on Windows, MicroPython QSTR generation reaches the
Windows command-line length limit. The Linux MicroPython and ESP-IDF vendor
checkouts are therefore kept in WSL Ubuntu 24.04. The build references the
project-owned board overlay at
`firmware/cyd_desktop_board` of the checkout, through its `/mnt/<drive>/...` path.

This does not move the runtime or applications into WSL. WSL is used only to
build firmware. The device, microSD, MCP bridge and authoritative Desktop-specific
sources remain associated with the Windows-side project.

## Next steps at this stage

Add the project-owned C/C++ shell component to this baseline. Initially implement
Home, Start, the single-foreground-app state machine and up to six logical icons.
LCD and touch drivers and the `cyd` Python API are to be integrated in later tasks.
