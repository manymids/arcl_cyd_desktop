# Third-party notices

English | [日本語](THIRD_PARTY_NOTICES_ja.md)

Original ARCL CYD Desktop code is provided under the MIT License in the root
[LICENSE](LICENSE). Third-party code and fonts retain their respective copyright
notices and license conditions. The project's MIT License does not replace them.

## Included external specification

[docs/external/arcl-common-spec_en.md](docs/external/arcl-common-spec_en.md) (Japanese:
[arcl-common-spec_ja.md](docs/external/arcl-common-spec_ja.md)) is the ARCL common
specification v0.5 from a separate project, included for reference. This project follows
it, but the document is not part of this project's work and the MIT License does not apply to it.

## Embedded font

The 5×7 glyphs in `firmware/cyd_desktop_shell/include/ui_font.h` derive from
Adafruit_GFX through TFT_eSPI's `Fonts/glcdfont.c`.

- Original notice: Copyright (c) 2012 Adafruit Industries. All rights reserved.
- License: BSD 2-Clause
- Upstream: <https://github.com/adafruit/Adafruit-GFX-Library>
- Source through which it was obtained: <https://github.com/Bodmer/TFT_eSPI>
- This project includes printable ASCII glyphs in array form.
- Original text: [Adafruit-GFX-BSD-2-Clause.txt](licenses/Adafruit-GFX-BSD-2-Clause.txt)
- TFT_eSPI's original notice documenting its multiple origins and conditions:
  [TFT-eSPI-license.txt](licenses/TFT-eSPI-license.txt)

Source distributions must retain the copyright notice, conditions and disclaimer.
Binary distributions containing the font must reproduce them in documentation
or other accompanying materials.

## Third-party components in the firmware

Taken from the link map `micropython.map` of the 2026-09-14 build (firmware 0.0.1): the
libraries that actually end up in the binary, checked against ESP-IDF's
[COPYRIGHT](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/COPYRIGHT.html) and
each component's license file.

| Component | License | Included license |
| --- | --- | --- |
| MicroPython v1.29.0 (core and bundled libraries; the unmodified source is included as the `firmware/micropython` submodule) | MIT and others, listed in its license | [MicroPython-LICENSE.txt](licenses/MicroPython-LICENSE.txt) |
| ESP-IDF v5.5.2 (Espressif code, including the Bluedroid Bluetooth host from Broadcom, `esp_hid`, the Wi-Fi, Bluetooth controller, PHY and coexistence binary libraries, and the `mdns` and `lan867x` components) | Apache-2.0 | [ESP-IDF-Apache-2.0.txt](licenses/ESP-IDF-Apache-2.0.txt) |
| Mbed TLS | Apache-2.0 | [ESP-IDF-Apache-2.0.txt](licenses/ESP-IDF-Apache-2.0.txt) |
| FreeRTOS Kernel | MIT | [FreeRTOS-Kernel-MIT.txt](licenses/FreeRTOS-Kernel-MIT.txt) |
| lwIP | BSD-3-Clause | [lwIP-BSD-3-Clause.txt](licenses/lwIP-BSD-3-Clause.txt) |
| wpa_supplicant (including parts derived from FreeBSD net80211) | BSD | [wpa_supplicant-BSD.txt](licenses/wpa_supplicant-BSD.txt) |
| newlib (C library) | BSD-style, several copyright holders | [newlib-COPYING.NEWLIB.txt](licenses/newlib-COPYING.NEWLIB.txt) |
| TLSF memory allocator | BSD-3-Clause | [TLSF-BSD-3-Clause.txt](licenses/TLSF-BSD-3-Clause.txt) |
| SD/MMC protocol definitions (from OpenBSD) | ISC | [SDMMC-OpenBSD-ISC.txt](licenses/SDMMC-OpenBSD-ISC.txt) |
| Xtensa HAL | MIT | [Xtensa-HAL-MIT.txt](licenses/Xtensa-HAL-MIT.txt) |
| TJpgDec (in the ESP32 mask ROM, not in the firmware binary) | ChaN's terms | [TJpgDec-license.txt](licenses/TJpgDec-license.txt) |

The MicroPython license includes Copyright (c) 2013-2026 Damien P. George and the
terms of its bundled libraries (littlefs, oofatfs and others). Where an ESP-IDF file
states a different copyright or license, the file takes precedence. The only frozen
Python modules are this project's `cyd.py`, `_boot.py` and `_manifest.py` and the
modules shipped with MicroPython's ESP32 port; no micropython-lib packages are included.

## Direct PC bridge dependencies

| Package | License | Included license |
| --- | --- | --- |
| `@modelcontextprotocol/sdk` | MIT | [MCP-SDK-MIT.txt](licenses/MCP-SDK-MIT.txt) |
| `serialport` | MIT | [SerialPort-MIT.txt](licenses/SerialPort-MIT.txt) |
| `zod` | MIT | [Zod-MIT.txt](licenses/Zod-MIT.txt) |

See `package-lock.json` for resolved dependencies. When distributing
`node_modules` or bundled dependency code, preserve the licenses, copyright
notices and required NOTICE files for every included package, including
transitive dependencies.

## Scope and binary distribution

This notice and `licenses/` collect the upstream license texts available locally
as of 2026-09-14. They are not an SBOM covering all linked release-firmware
components or a certificate of a completed license audit.

When distributing `micropython.bin` or other firmware binaries, include this
notice and `licenses/`. Check the actual ESP-IDF/MicroPython third-party
components in that build and add any required copyright, license and NOTICE
materials. Recheck the distribution when dependency sources or build settings change.

Reference: <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/COPYRIGHT.html>

The project's MIT License does not grant rights to third-party images or data.
