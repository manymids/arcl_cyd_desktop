# ARCL CYD Desktop

[English](README_en.md) | 日本語

ESP32-2432S028 CYD 向けの小型デスクトップ環境です。タッチ操作に加え、USBシリアルとMCPから状態確認、アプリの起動、入力、ファイル転送を行えます。ESP-IDF上のネイティブシェルにMicroPythonを組み込み、microSD上のアプリを実行します。

## 主な機能

- Home／Start、時計・カレンダー、Scripts、設定、テキストエディタ、画面キーボード
- 無線モードの切替（OFF／Wi-Fi／Bluetooth。排他で、切替は再起動）
- Bluetoothキーボード：ペアリング画面、JIS／US配列、エディタ・コンソール・アプリへのキー入力、接続中は画面キーボードを隠す
- Scriptsから実行したスクリプトの `print()` 出力とエラーを画面に表示、Ctrl+Cで中断
- NVSへのタッチ較正保存、FAT/FAT32のmicroSDマウント
- MicroPythonアプリのインストール・起動・削除、Homeショートカット
- USB JSON Linesによる状態取得・入力操作、stdio MCP bridge
- UIツリー、診断、ログ、例外トレースバック、協調的なpause／step／resume
- 2D描画、ゲーム用スプライト、JPEG表示、MJP1動画再生
- Native App SDK、組込みレイキャストデモ `neon3d`、MJPプレイヤー `mjpplayer`

LCDは320×240です。通常のUI描画は最大320×16のタイルを転送し、全画面のフレームバッファを必要としない構成です。画面ミラーはUIツリーからPC側で再構成するPNGのため、LCD上のすべての描画を再現するものではありません。

本体はMITライセンスです。詳しくは末尾の「ライセンス」を参照してください。

## 開発者向けドキュメント

- [公開ドキュメント一覧](docs/README_ja.md)：各資料の用途と過去の記録を読む際の注意
- [cyd APIリファレンス](docs/cyd-api_ja.md)：MicroPythonアプリの開発・インストール
- [ネイティブアプリの追加手順](docs/native-app-guide_ja.md)：C++アプリの実装・登録・ビルド
- [ビルド環境を一から作る](docs/build_ja.md)：WSL、ESP-IDF v5.5.2の導入、同梱したMicroPython v1.29.0（サブモジュール）の取得から書き込みまで
- [ビルド環境の基線](docs/p2-build-baseline_ja.md)：ESP-IDF／MicroPythonとWSL構成を決めた経緯（開発段階の記録）
- [MCP操作・観測インターフェース](docs/p13-arcl-interface_ja.md)：UIツリー・入力・診断・実行制御
- [MCPツール一覧](docs/mcp-tools_ja.md)：全ツールの種別・レイヤ・引数

## 対象環境

| 項目 | 内容 |
| --- | --- |
| ボード | ESP32-2432S028 CYD（ILI9341版）だけ。下の「対応ハードウェア」を参照 |
| PC側 | Node.jsとnpm。ホスト側検証ではNode.js 22.17.0を使用 |
| 接続 | USBシリアル、通常115200 baud、8N1 |
| ストレージ | FAT/FAT32形式のmicroSD |
| キーボード（任意） | Bluetooth Classic（HID）のキーボード。BLE専用のキーボードとマウスは非対応 |
| ファームウェア開発環境 | WSL Ubuntu 24.04、ESP-IDF、MicroPython |

## 対応ハードウェア

対応するのは、**ILI9341の液晶と抵抗膜タッチ（XPT2046）を載せたESP32-2432S028（通称CYD、Cheap Yellow Display）** だけです。ピン配置、液晶の初期化、向き（横長320×240、BGR）はファームウェアに固定しており、設定で切り替える仕組みはありません。

| 部品 | 仕様・接続 |
| --- | --- |
| SoC | ESP32-D0WD-V3、Flash 4 MiB、PSRAMなし |
| 液晶 | ILI9341 2.8インチ 320×240、SPI2：SCK=14、MOSI=13、MISO=12、CS=15、DC=2、バックライト=21 |
| タッチ | XPT2046（抵抗膜、ビットバング）：CLK=25、MOSI=32、MISO=39、CS=33、IRQ=36 |
| microSD | SCK=18、MOSI=23、MISO=19、CS=5（`machine.SDCard(slot=2)`） |
| USBシリアル | CH340（VID `1a86`／PID `7523`） |

次の基板は**対応していません**：液晶がST7789の版（USB端子が2つあるCYD2USBなど）、静電容量式タッチの版、画面サイズの違う派生品（ESP32-3248S035など）。書き込んでも表示が崩れる・色が反転する・タッチが効かない、といった状態になります。

**初回起動**：タッチ補正が保存されていない基板では、起動するとすぐに補正画面になります。3か所に順に表示される十字の中心をタップすると保存され、Homeが表示されます。やり直すときは Settings > SYSTEM > CALIBRATE（MCPでは `cyd_touch_calibrate_start`）です。補正はNVSに保存され、アプリ領域だけの書き込みでは消えません。

MicroPythonアプリは単一前景で実行します。「Sandbox」はセキュリティ上の隔離を意味しません。アプリはMicroPythonのファイル・ハードウェアAPIを利用できるため、信頼できるアプリを実行してください。

## PC MCP bridge

リポジトリ直下で依存パッケージをインストールします。

```powershell
npm ci
$env:CYD_DESKTOP_PORT = "COM6" # 実際の接続先に置き換える
npm start
```

ポートを指定しない場合はCH340（VID `1a86` / PID `7523`）を自動検出します。複数のCH340機器がある場合はポートを明示してください。MCPクライアントからは、このフォルダを作業ディレクトリとして `node bridge/index.mjs` を起動します。通信方式はstdioで、ネットワーク待受けは行いません。

ツール名は [ARCL 共通仕様](docs/external/arcl-common-spec_ja.md)（別プロジェクトの外部仕様。v0.5 を同梱）の命名規約に従い、共通 API の `arcl_status` と、CYD 固有の `cyd_*` に分かれます。主なツールは `arcl_status`、`cyd_launch`、`cyd_home`、`cyd_tap`、`cyd_input_macro`、`cyd_key_press`、`cyd_type_text`、`cyd_ui_tree`、`cyd_settings_get/set`、`cyd_radio_status/set`、`cyd_bt_status/scan/connect/forget`、`cyd_logs`、`cyd_diagnostics`、`cyd_app_error`、`cyd_pause/step/resume`、`cyd_screen_mirror` です。

- `arcl_status` は `machine: "cyd"`、有効なレイヤ、現在の画面を返します。
- CYD はエミュレータではなく実機です。シェル、ネイティブアプリ、動画、時計は実時間で動き続け、止められません。フレーム番号とステート保存はありません（`arcl_status` の `frame` は `null`、`time_control` は `"none"`）。`cyd_pause/step/run` が止めるのは、前面の MicroPython アプリの `cyd.update()` だけです。
- 公開するツールは能力レイヤで絞り込めます。`node bridge/index.mjs --mcp-layers=L0,L1`（または環境変数 `CYD_MCP_LAYERS`）。L0 は画面と入力（キー入力を含む）、L1 はアプリ・設定・ファイル、L2 はログと診断、L3 は SD・タッチ較正・無線モード・Bluetooth です。`arcl_status` と `cyd_pause` などの Control 系は常に公開されます。指定しなければ全レイヤを公開します。

全ツールの種別・レイヤ・引数は [MCP ツール一覧](docs/mcp-tools_ja.md) にあります。MCP のツール名とは別に、ファームウェアのシリアルプロトコルのコマンドは `desktop_*` のままです（下の JSON Lines の例）。

同じシリアルポートをMCP bridge、転送コマンド、書込みツールなどで同時に開かないでください。

## microSDとアプリ

FAT/FAT32のmicroSDを電源投入前に挿入してください。起動時に `machine.SDCard(slot=2)`（SCK=18、MOSI=23、MISO=19、CS=5）で `/sd` にマウントします。マウント失敗時に自動フォーマットはしません。`cyd_sd_status` で状態を確認できます。カード交換後は再起動が必要です。

アプリは `/sd/apps/<app-id>/` に配置します。次の2ファイルをPC側の `apps/hello/` に作成すると、画像を使わない最小アプリを試せます。

`manifest.json`:

```json
{
  "id": "hello",
  "title": "HELLO",
  "entry": "main.py",
  "version": "0.0.1"
}
```

`main.py`:

```python
import cyd
import time

cyd.clear()
cyd.title("HELLO")
cyd.text(16, 60, "Hello, CYD!")

while cyd.update():
    time.sleep_ms(20)
```

bridgeを終了してポートを空けてから、ディレクトリ単位で転送します。

```powershell
npm run deploy -- apps/hello --pin --launch
```

manifestはPC側で検査され、ファイルごとにPCとデバイスでSHA-256を確認します。既定では同名ファイルを置換します。置換を禁止する場合は `--no-replace` を指定してください。転送前にHomeへ戻ります。アプリの転送はフラットなディレクトリに対応し、サブディレクトリの再帰転送には対応していません。

MCPの `cyd_app_deploy` では、アプリを配置する親フォルダを指定して起動します。

```powershell
$env:CYD_DESKTOP_APPS_ROOT = Join-Path (Get-Location) "apps"
npm start
```

この状態で `app_dir: "hello"` を指定すると、`apps/hello` を転送できます。未設定時の検索ルートは `examples/` です。公開版でサンプルを含めない場合は、上記のように自分のアプリフォルダを設定してください。1ファイルの転送には `cyd_package_upload` を使い、バイナリは `encoding: "base64"` で渡せます。

アプリは `cyd.update()` を定期的に呼び、入力処理・描画・停止要求を処理します。戻り値が `False` になったらループを終了してください。keepaliveを行わない無限ループは、15秒のTask Watchdogによる再起動の対象です。ストレージ値の書込み上限は512バイト、JSON checkpointは4096バイトです。Wi-Fi／HTTP補助APIは無線モードがWi-Fiのときだけ使えます（それ以外では理由付きの `RuntimeError`）。現在の `cyd.http_get()` はHTTPのみで、HTTPSには対応していません。

画面に何も描かないスクリプトの `print()` 出力は画面に表示され、例外で止まったときはトレースバックが画面に残ります（Homeで閉じる）。

## 無線モードとBluetoothキーボード

Wi-FiとBluetoothはメモリに同時に載らないため、どちらを使うかを **Settings > WIRELESS** で選びます。初期値はOFFです。別のモードを選ぶと確認が出て、再起動後に切り替わります。Home表示中の空きヒープの目安は、OFF／Wi-Fiで約146 KB、Bluetoothで約54 KBです。

| モード | 使えるもの |
| --- | --- |
| OFF | Wi-FiもBluetoothも使わない。メモリが最も多く残る |
| WI-FI | `cyd.wifi_*`、`cyd.http_get()`、`network`／`espnow`（接続先は `/sd/wifi.json`）。他のモードではファームウェアがWi-Fiの起動を拒否します |
| BLUETOOTH | Bluetoothキーボード |

### ペアリング

1. Settings > WIRELESS で **BLUETOOTH** を選び、再起動します。
2. Wirelessページの **KEYBOARD >** を開き、キーボードをペアリングモードにしてから **SCAN** を押します。
3. 見つかったキーボードの行をタップします。画面に大きく出た数字（毎回ランダム）をキーボードで打ち、Enterを押します。
4. 「PAIRED」と出たら完了です。**KEYS JIS／KEYS US** でキー配列を選びます（初期値はJIS）。

キーボードは1台だけ記憶します。別のキーボードをペアリングすると前のキーボードは忘れ、**FORGET** で記憶を消せます。スリープしたキーボードはキーを押すと自動で再接続しますが、つながるまでに押したキーは届きません。タスクバー右端の記号は、接続中が緑、ペアリング済みで未接続が黄色で、接続・切断の瞬間には3秒間の通知が出ます。

### キーの使い方

- **エディタ**：接続中は画面キーボードを隠して16行を表示します。矢印、Home／End、PageUp／PageDown、Tab／Shift+Tab、Enter（字下げを引き継ぎ、`:` の後は1段深く）、Delete、Ctrl+S（保存）、Ctrl+O（開く）が使えます。1ファイル約1 KBまでです。
- **コンソール**（`examples/console`）：接続中は21行表示になり、↑↓で履歴、`:` で始まるブロックは空行で実行、Ctrl+Cで入力の取消や実行中コードの中断ができます。
- **アプリ**：`cyd.key()` でキーを受け取ります。Ctrl+Cは実行中のアプリに `KeyboardInterrupt` を送ります。詳しくは [cyd APIリファレンス](docs/cyd-api_ja.md) の「ハードウェアキーボード」を参照してください。

### Bluetoothモードの制限

- Bluetooth用に24 KBを残すため、MicroPythonのヒープはそれ以上に広がりません。大きなメモリを使うアプリは `MemoryError` で止まります。
- MJPプレイヤーの動画は再生できません（約49 KB必要）。OFFかWi-Fiモードで再生してください。
- 対応するのはBluetooth Classicのキーボードです。このファームウェアで試した機種はSKB-BT23BKです（KB-PLT8990-Kは開発中の検証用ファームウェアでキー入力まで確認）。

## USB JSON Lines

UART0はUTF-8 JSON Lines専用です。対話REPLは無効化しています。各JSONオブジェクトを改行で区切って送ります。

```json
{"id":"status-1","command":"desktop_status"}
{"id":"clock-1","command":"desktop_launch","app_id":"clock"}
{"id":"game-1","command":"desktop_launch","app_id":"neon3d"}
{"id":"home-1","command":"desktop_home"}
{"id":"sd-1","command":"desktop_sd_status"}
{"id":"script-1","command":"desktop_script_launch","app_id":"hello"}
{"id":"tree-1","command":"desktop_ui_tree","offset":0,"limit":4}
{"id":"radio-1","command":"desktop_radio_status"}
{"id":"key-1","command":"desktop_key","key":"CTRL+S"}
{"id":"pause-1","command":"desktop_pause"}
{"id":"step-1","command":"desktop_step"}
{"id":"resume-1","command":"desktop_resume"}
```

`hello` の起動例は、上記アプリのインストール後に利用できます。

## ファームウェアのビルドと書込み

ビルドせずに書き込むだけなら、公開リポジトリの Releases にあるビルド済みの zip を使えます。手順とリリースの作り方は [ビルド済みファームウェア](docs/firmware-release_ja.md) にあります。

ESP-IDF v5.5.2 と MicroPython v1.29.0 でビルドします。MicroPython のソースは、上流の v1.29.0 を無改変のまま **`firmware/micropython` にサブモジュールとして同梱**しています（取得は `git submodule update --init firmware/micropython`）。ESP-IDF とツールチェーンは同梱せず、別に用意します。

WindowsでのQSTR生成時のコマンド行長制限を避けるため、WSLでビルドします。以下は**構築済みの開発環境での増分ビルド手順**です。WSLの用意、ESP-IDFの導入、サブモジュールの取得、初回設定は [ビルド環境を一から作る](docs/build_ja.md) にあります。

Windows側でこのリポジトリのルートを開き、次を実行します（WSLはWindowsのカレントディレクトリで起動します）。

```powershell
wsl -d Ubuntu-24.04 -e bash -c 'export IDF_TOOLS_PATH=~/cyd-desktop-vendor/.idf-tools && . ~/cyd-desktop-vendor/esp-idf/export.sh > /dev/null && cd firmware/micropython/ports/esp32 && idf.py -B build-cyd_desktop_board build'
```

ビルド設定は、初回に次のパスを指定して作られています（詳しくはビルド手順の資料）。

| 設定 | このプロジェクト内の参照先 |
| --- | --- |
| `MICROPY_BOARD_DIR` | `firmware/cyd_desktop_board` |
| `USER_C_MODULES` | `firmware/cyd_desktop_shell/micropython.cmake` |

シェルは `screen_*.cpp` などへ分割されており、コンパイル対象の正本は `firmware/cyd_desktop_shell/micropython.cmake` です。

ビルド結果を、同じくリポジトリのルートへコピーします。

```powershell
wsl -d Ubuntu-24.04 -e bash -c 'b=firmware/micropython/ports/esp32/build-cyd_desktop_board; cp "$b/micropython.bin" "$b/bootloader/bootloader.bin" "$b/partition_table/partition-table.bin" .'
```

ファームウェアは Bluetooth を含むため、アプリパーティションは 3 MB です（`firmware/cyd_desktop_board/partitions-cyd.csv`）。未書込みの基板と、Bluetooth 対応より前のファームウェアが入った基板では、bootloader とパーティションテーブルも書き込みます。ポートを確認し、他のシリアル接続を閉じてから実行してください。NVS の位置は変わらないので、タッチ補正と設定は残ります。

```powershell
python -m esptool --port COM6 --before default_reset write_flash 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 micropython.bin
```

以降はアプリパーティションだけの更新で足ります。

```powershell
python -m esptool --port COM6 --before default_reset write_flash 0x10000 micropython.bin
```

## 開発用テストの現状

次のコマンドでホスト側検証を実行できます。実機は不要です。

```bash
npm test
# C++とPythonの検証。WSLにg++とpython3が必要（npm run test:native でも可）
wsl bash tools/run-native-tests.sh
```

2026-09-14の確認では、JavaScript 59件、C++ 7組、Pythonのmanifest検証が成功し、スキップは0件でした。`npm audit` の既知脆弱性は0件でしたが、これは確認時点のnpm依存に対する結果です。ファームウェア全体の安全性を保証するものではありません。

ネイティブテストスクリプトはコンパイル失敗を `SKIP` として扱うため、終了コードだけでなく `failed` と `skipped` の両方を確認してください。g++が無い環境（WindowsのGit Bashなど）では全件が `SKIP` になります。

## ライセンス

本体の独自コードは **MIT License — Copyright (c) 2026 manymids** です。

- [LICENSE](LICENSE)：本体のMITライセンス
- [THIRD_PARTY_NOTICES_ja.md](THIRD_PARTY_NOTICES_ja.md)：第三者コードの由来・条件・配布時の注意
- [licenses/](licenses/)：第三者ライセンスの原文

Adafruit由来フォントはBSD 2-Clause、MicroPython本体はMITです。第三者由来部分にはそれぞれのライセンスが適用されます。ファームウェアのバイナリを配布する場合はライセンス資料を添付し、実際のビルドに含まれる追加部品の表示も確認してください。現在の資料は、全リンク部品のライセンス監査完了を示すものではありません。
