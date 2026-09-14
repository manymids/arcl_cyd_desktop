# Third-party notices

[English](THIRD_PARTY_NOTICES_en.md) | 日本語

ARCL CYD Desktop の独自コードは、ルートの [LICENSE](LICENSE) に記載した MIT
License で提供します。第三者由来のコード・フォントは、それぞれの著作権表示と
ライセンスに従います。本体の MIT License は、第三者の表示や条件を置き換えません。

## 同梱している外部仕様

[docs/external/arcl-common-spec_ja.md](docs/external/arcl-common-spec_ja.md)（英語版
[arcl-common-spec_en.md](docs/external/arcl-common-spec_en.md)）は、別プロジェクトの
ARCL 共通仕様 v0.5 を参照用に同梱したものです。このプロジェクトはこの仕様に準拠していますが、
文書そのものはこのプロジェクトの成果物ではなく、本体の MIT License の対象外です。

## 組み込まれたフォント

`firmware/cyd_desktop_shell/include/ui_font.h` の 5×7 グリフは、TFT_eSPI の
`Fonts/glcdfont.c` を経由した Adafruit_GFX 由来のフォントです。

- 原著作権表示: Copyright (c) 2012 Adafruit Industries. All rights reserved.
- ライセンス: BSD 2-Clause
- 上流: <https://github.com/adafruit/Adafruit-GFX-Library>
- 取得経路: <https://github.com/Bodmer/TFT_eSPI>
- このプロジェクトでは印字可能 ASCII のグリフを配列形式で収録しています。
- 原文: [Adafruit-GFX-BSD-2-Clause.txt](licenses/Adafruit-GFX-BSD-2-Clause.txt)
- TFT_eSPI の複数の由来と条件を記録した原文:
  [TFT-eSPI-license.txt](licenses/TFT-eSPI-license.txt)

ソース配布では著作権表示・条件・免責条項を保持してください。フォントを含む
バイナリ配布でも、これらを配布文書または同梱資料に含めてください。

## ファームウェアに含まれる第三者部品

2026-09-14 のビルド（ファームウェア 0.0.1）のリンクマップ `micropython.map` から、
実際にバイナリへ入ったライブラリを抽出し、ESP-IDF の
[COPYRIGHT](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/COPYRIGHT.html) と
各部品のライセンスファイルで確認した一覧です。

| 部品 | ライセンス | 原文 |
| --- | --- | --- |
| MicroPython v1.29.0（コアと同梱ライブラリ。ソースを `firmware/micropython` にサブモジュールとして無改変で同梱） | MIT ほか（原文に列挙） | [MicroPython-LICENSE.txt](licenses/MicroPython-LICENSE.txt) |
| ESP-IDF v5.5.2（Espressif の独自コード。Bluetooth ホストの Bluedroid〔Broadcom 由来〕、`esp_hid`、Wi-Fi・Bluetooth コントローラ・PHY・共存制御のバイナリライブラリ、`mdns` と `lan867x` 部品を含む） | Apache-2.0 | [ESP-IDF-Apache-2.0.txt](licenses/ESP-IDF-Apache-2.0.txt) |
| Mbed TLS | Apache-2.0 | [ESP-IDF-Apache-2.0.txt](licenses/ESP-IDF-Apache-2.0.txt) |
| FreeRTOS Kernel | MIT | [FreeRTOS-Kernel-MIT.txt](licenses/FreeRTOS-Kernel-MIT.txt) |
| lwIP | BSD-3-Clause | [lwIP-BSD-3-Clause.txt](licenses/lwIP-BSD-3-Clause.txt) |
| wpa_supplicant（FreeBSD net80211 由来部分を含む） | BSD | [wpa_supplicant-BSD.txt](licenses/wpa_supplicant-BSD.txt) |
| newlib（C ライブラリ） | BSD 系（複数の権利者） | [newlib-COPYING.NEWLIB.txt](licenses/newlib-COPYING.NEWLIB.txt) |
| TLSF メモリアロケータ | BSD-3-Clause | [TLSF-BSD-3-Clause.txt](licenses/TLSF-BSD-3-Clause.txt) |
| SD/MMC プロトコル定義（OpenBSD 由来） | ISC | [SDMMC-OpenBSD-ISC.txt](licenses/SDMMC-OpenBSD-ISC.txt) |
| Xtensa HAL | MIT | [Xtensa-HAL-MIT.txt](licenses/Xtensa-HAL-MIT.txt) |
| TJpgDec（ESP32 のマスク ROM 内。ファームウェアのバイナリには含まれない） | ChaN の条件 | [TJpgDec-license.txt](licenses/TJpgDec-license.txt) |

MicroPython の原文には Copyright (c) 2013-2026 Damien P. George と、同梱ライブラリ
（littlefs、oofatfs など）の条件が含まれています。ESP-IDF の各ファイルの著作権表示が
この表と異なる場合は、ファイルの表示が優先します。ボード定義で凍結している Python モジュールは、
このプロジェクトの `cyd.py`・`_boot.py`・`_manifest.py` と、MicroPython の ESP32 ポート付属の
モジュールだけです（micropython-lib のパッケージは含めていません）。

## PC bridge の直接依存

| Package | License | Included license |
| --- | --- | --- |
| `@modelcontextprotocol/sdk` | MIT | [MCP-SDK-MIT.txt](licenses/MCP-SDK-MIT.txt) |
| `serialport` | MIT | [SerialPort-MIT.txt](licenses/SerialPort-MIT.txt) |
| `zod` | MIT | [Zod-MIT.txt](licenses/Zod-MIT.txt) |

依存関係の解決結果は `package-lock.json` を参照してください。`node_modules` や
依存コードをバンドルして配布する場合は、間接依存も含め、配布する各パッケージの
ライセンス・著作権表示・必要な NOTICE を保持してください。

## この資料の範囲とバイナリ配布

本資料と `licenses/` は、2026-09-14 時点で手元にある依存元のライセンスを
収録したものです。リリース用ファームウェアの全リンク部品を検証した SBOM
またはライセンス監査完了証明ではありません。

`micropython.bin` などを配布するときは、本資料と `licenses/` を添付し、さらに
そのビルドに実際に含まれる ESP-IDF/MicroPython の第三者部品について、必要な
著作権表示・ライセンス・NOTICE を確認して追加してください。依存ソースや
ビルド設定を更新した場合も、配布内容に合わせて再確認してください。

参考: <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/COPYRIGHT.html>

本体の MIT License を第三者の画像やデータの利用許諾として解釈しないでください。
