# Public documentation

English | [日本語](README_ja.md)

This directory contains documentation for users and application developers.
Each document is available as a Japanese and English pair.

| Document | Purpose |
| --- | --- |
| [cyd API reference](cyd-api_en.md) | MicroPython app layout, manifests, installation, UI, games, images, console, hardware keyboard, storage, radio mode and networking |
| [Native app development guide](native-app-guide_en.md) | Implementing and registering C++ apps, build integration, constraints and tests |
| [Build from scratch](build_en.md) | WSL, installing ESP-IDF v5.5.2, fetching the included MicroPython v1.29.0 submodule, the first build, flashing and tests |
| [Prebuilt firmware](firmware-release_en.md) | Contents of the Releases zip, checking SHA-256, flashing with esptool, and making a release with `npm run package:firmware` |
| [Build baseline](p2-build-baseline_en.md) | Baseline ESP-IDF/MicroPython versions and the reasons for the WSL setup |
| [MCP tools](mcp-tools_en.md) | Every tool the MCP bridge publishes, with category, layer, arguments and description (generated from the bridge) |
| [ARCL common specification v0.5](external/arcl-common-spec_en.md) | External specification from a separate project, included for reference and not covered by the MIT License; the MCP tool names and layers follow it |
| [MCP control and observation interface](p13-arcl-interface_en.md) | UI trees, input, diagnostics, cooperative execution control and checkpoints |

See the [project README](../README_en.md) for connection settings, a minimal app
without image assets, radio mode and Bluetooth keyboards, and the current
incremental build and flashing procedure.

## Notes on the existing documents

- The build baseline and P13 interface documents also record particular stages
  of development. Firmware sizes, heap measurements, port numbers and test
  results describe those stages, not current specifications or performance guarantees.
