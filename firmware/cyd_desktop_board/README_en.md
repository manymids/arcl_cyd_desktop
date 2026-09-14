# ARCL CYD Desktop board overlay

English | [日本語](README_ja.md)

This directory is the project-owned MicroPython board definition.  The
MicroPython source is included as the `firmware/micropython` submodule (upstream v1.29.0,
unmodified); ESP-IDF is not included and is installed separately.

| File | Role |
| --- | --- |
| `mpconfigboard.cmake` / `mpconfigboard.h` | Board settings: Bluetooth, the initial GC heap (32 KB) and the partition table |
| `sdkconfig.bt` | ESP-IDF settings for the Bluetooth Classic HID host (no BLE) |
| `partitions-cyd.csv` | 4 MiB flash with a 3 MB app partition; NVS keeps its usual offset |
| `components/cyd_bt_keyboard/` | Builds `cyd_desktop_shell/bt_keyboard.cpp` as an IDF component (linking bt into the user module makes the QSTR command line too long), and wraps the GC heap growth limit |
| `modules/` | Frozen Python modules (`cyd.py`, `_boot.py`, `_manifest.py`) |
| `board_startup.c` | Starts the shell, the protocol and Bluetooth at boot |
