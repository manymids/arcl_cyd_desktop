# P2: ESP-IDF / MicroPython build baseline

[English](p2-build-baseline_en.md) | 日本語

実施日: 2026-09-07

## 成果

CYD Desktop Sandbox用のMicroPython custom boardを、ESP32向けに正常ビルドした。

| 項目 | 値 |
| --- | --- |
| MicroPython | v1.29.0 |
| ESP-IDF | v5.5.2 |
| 対象 | ESP32-D0WD-V3 / Flash 4 MiB / PSRAMなし |
| 結合ファームウェア | 759 KiB |
| board overlay | `firmware/cyd_desktop_board` |
| 成果物 | `~/cyd-desktop-vendor/micropython/ports/esp32/build-cyd_desktop_board/firmware.bin`（WSL） |

## ビルド環境

WindowsのESP-IDF CMake/Ninjaでは、MicroPythonのQSTR生成がWindowsのコマンド行長制限に
達する。Linux版のMicroPython/ESP-IDF vendor checkoutをWSL Ubuntu 24.04に置き、
プロジェクト所有のboard overlayをチェックアウトの `firmware/cyd_desktop_board`（WSL からの `/mnt/<ドライブ>/...` のパス）として
参照してビルドする。

これは実行時やアプリの場所をWSLへ移すものではない。WSLはファームウェアのビルド専用であり、
実機、microSD、MCP bridge、Desktop固有ソースの正本は引き続きWindows側プロジェクトに置く。

## 次

このbaselineへproject-ownedのC/C++ shell componentを追加する。最初はHome、Start、単一前景
アプリの状態機械と、論理的な最大6アイコンだけを実装する。LCD・タッチドライバおよび`cyd`
Python APIは後続タスクで統合する。
