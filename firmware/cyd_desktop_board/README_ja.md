# CYD Desktop Sandbox ボード定義

[English](README_en.md) | 日本語

このディレクトリには、プロジェクト独自のMicroPythonボード定義を置いています。
MicroPython のソースは `firmware/micropython` にサブモジュール（上流の v1.29.0、無改変）として含めています。ESP-IDF は同梱せず、別に導入します。

| ファイル | 役割 |
| --- | --- |
| `mpconfigboard.cmake` / `mpconfigboard.h` | ボード設定。Bluetooth、GCヒープの初期サイズ（32 KB）、パーティション表の指定 |
| `sdkconfig.bt` | Bluetooth Classic HIDホスト（BLEなし）のESP-IDF設定 |
| `partitions-cyd.csv` | 4 MiBフラッシュ用。アプリ領域3 MB、NVSは従来と同じ位置 |
| `components/cyd_bt_keyboard/` | `cyd_desktop_shell/bt_keyboard.cpp` をIDF部品としてビルドする（btを直接リンクするとQSTR生成のコマンド行が長すぎるため）。GCヒープ拡張の上限もここで差し替える |
| `modules/` | 凍結するPythonモジュール（`cyd.py`、`_boot.py`、`_manifest.py`） |
| `board_startup.c` | 起動時にシェル、プロトコル、Bluetoothを開始する |
