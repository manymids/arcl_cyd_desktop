# Prebuilt firmware

English | [日本語](firmware-release_ja.md)

The firmware is published on the public repository's Releases page as a zip file
(`cyd-desktop-firmware-<version>.zip`). It can be flashed with that zip alone; no build
environment is needed.

## Flashing (users)

Only the **ESP32-2432S028 (CYD) with the ILI9341 panel** is supported (see
[Supported hardware](../README_en.md#supported-hardware)).

### What is in the zip

| File | Contents |
| --- | --- |
| `bootloader.bin`, `partition-table.bin`, `micropython.bin` | The three images for 0x1000, 0x8000 and 0x10000 |
| `cyd-desktop-<version>-full.bin` | The three combined into one image for 0x0. **It also erases the settings area (NVS)**, so it is meant for blank boards |
| `FLASHING.md` | Flashing steps in Japanese and English |
| `BUILD-INFO.json` | Version, source commit, MicroPython commit, ESP-IDF version and each file's SHA-256 |
| `SHA256SUMS` | SHA-256 of each image |
| `LICENSE`, `THIRD_PARTY_NOTICES*.md`, `licenses/` | Licenses of the project and third-party software |

### Steps

1. Install esptool: `pip install esptool`
2. Extract the zip and check that the files are intact:

   ```powershell
   Get-FileHash micropython.bin   # compare with SHA256SUMS
   ```

3. Connect over USB, find the port (for example `COM6`) and close other serial connections.
4. Flash according to the board's state:

   | State | Command |
   | --- | --- |
   | Blank, or running older firmware (touch calibration and settings are kept) | `python -m esptool --chip esp32 --port COM6 --before default_reset write_flash 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 micropython.bin` |
   | Updating from this firmware | `python -m esptool --chip esp32 --port COM6 --before default_reset write_flash 0x10000 micropython.bin` |
   | One file (for example a browser-based flasher; **settings are erased**) | Write `cyd-desktop-<version>-full.bin` at 0x0 |

5. A board without a stored touch calibration shows a crosshair at three places in turn
   at boot; tap the centre of each. Choose the radio (Wi-Fi or Bluetooth) in
   Settings > WIRELESS (OFF by default).

## Making a release (maintainers)

A binary must always match the source it was built from. Build from a clean checkout
of the public repository.

1. In the public repository, set the version in `package.json`, `package-lock.json`,
   `firmware/cyd_desktop_shell/include/desktop_version.h` and `bridge/index.mjs`
   (`npm test` fails if they disagree). Commit and tag it (for example `v0.0.1`).
2. Check out that commit, with its submodule, at a **path of 55 characters or fewer**
   and build it as in [Setting up the firmware build from scratch](build_en.md).
3. Copy the build output to the repository root and make the package:

   ```powershell
   wsl -d Ubuntu-24.04 -e bash -c 'b=firmware/micropython/ports/esp32/build-cyd_desktop_board; cp "$b/micropython.bin" "$b/bootloader/bootloader.bin" "$b/partition_table/partition-table.bin" .'
   npm run package:firmware
   ```

   This writes `dist/cyd-desktop-firmware-<version>.zip` and prints each file's SHA-256.
   The three copied binaries and `dist/` are in `.gitignore`.
   - It refuses when there are uncommitted changes or files Git does not track (for a
     trial package use `-- --allow-dirty`; `source_dirty` in `BUILD-INFO.json` is then `true`).
   - The combined image is made with esptool's `merge_bin`, so `python -m esptool` must
     work. Where it does not, leave the image out with `-- --no-merged`.
4. Flash it and check that it works; try at least the blank-board (three files) and the
   update steps.
5. Create the release for the tag on GitHub Releases and attach the zip. In the release
   notes, give the version, the supported hardware, the printed SHA-256 values and the changes.

### Why the license files are included

The firmware contains MicroPython (MIT), ESP-IDF (Apache-2.0), FreeRTOS (MIT), lwIP (BSD),
newlib (BSD-style) and more. Their licenses require the copyright notices and license
texts to accompany binary distributions, so the package always includes `LICENSE`,
`THIRD_PARTY_NOTICES*.md` and `licenses/`. After changing dependencies or build settings,
review the list in the [third-party notices](../THIRD_PARTY_NOTICES_en.md).
