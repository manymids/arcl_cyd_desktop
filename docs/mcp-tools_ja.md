# MCP ツール一覧

[English](mcp-tools_en.md) | 日本語

MCP bridge（`node bridge/index.mjs`）が公開するツールの一覧です。
**この文書は `npm run docs:tools` で bridge の定義から生成しています。直接編集しないでください。**
定義と食い違うと `bridge/mcp-tools-doc.test.mjs` が失敗します。

- **名前**: [ARCL 共通仕様](external/arcl-common-spec_ja.md)（外部仕様）の命名規約に従い、共通 API は `arcl_*`、CYD 固有のものは `cyd_*` です。
- **種別**: Observation は状態を読むだけ、Action は状態を変える、Control は実行を制御します。
- **レイヤ**: `node bridge/index.mjs --mcp-layers=L0,L1`（または環境変数 `CYD_MCP_LAYERS`）で、公開するレイヤを選べます。指定しなければ全レイヤです。Control 系はレイヤに関係なく常に公開されます。
- **時間**: CYD はエミュレータではなく実機です。シェル、ネイティブアプリ、動画、時計は実時間で動き続けます。`cyd_pause` などで止められるのは、前面の MicroPython アプリの `cyd.update()` だけです。
- **プロトコルとの関係**: ファームウェアのシリアルプロトコルのコマンドは `desktop_*` という名前です。MCP のツール名とは別物です。

全 42 ツール。

## Control（常に公開）

| ツール | 種別 | 引数 | 説明 |
| --- | --- | --- | --- |
| `arcl_status` | Control | なし | 機種 ID（`cyd`）、有効なレイヤ、現在の画面と前面アプリ、MicroPython アプリの実行状態を返す。実機なので `running` は常に true、`frame` は `null`、`time_control` は `"none"`。 |
| `cyd_run` | Control | `frames`: 整数、1〜3600 | MicroPython アプリを止めたうえで、`cyd.update()` を `frames` 回分進めてよいことにする。進み終わるのを待たずに返る。 |
| `cyd_runtime_status` | Control | なし | 前面の MicroPython アプリの一時停止状態、`cyd.update()` の回数（`frame`）、残りのステップ数を返す。 |
| `cyd_pause` | Control | なし | 前面の MicroPython アプリを、次の `cyd.update()` で止める。ネイティブのシェルやアプリ、動画、時計は止まらない。 |
| `cyd_resume` | Control | なし | 止めていた MicroPython アプリを再開する。 |
| `cyd_step` | Control | なし | MicroPython アプリを止めたうえで、`cyd.update()` を 1 回だけ進める。 |

## L0 画面と入力

| ツール | 種別 | 引数 | 説明 |
| --- | --- | --- | --- |
| `cyd_ui_tree` | Observation | なし | 今表示中の画面の UI ノード（id、役割、ラベル、位置と大きさ、有効かどうか）をすべて返す。画面の状態を知るいちばん軽い方法。 |
| `cyd_tap` | Action | `x`: 整数、0〜319<br>`y`: 整数、0〜239 | 画面座標にタップを 1 回キューに入れる。 |
| `cyd_input_state` | Observation | なし | タッチが今押されているか、最後の座標、キューに残っている合成タップの数を返す。 |
| `cyd_input_macro` | Action | `steps`: 配列、1〜64 件、各要素は {x, y, delay_ms?} | タップを最大 64 回、指定した待ち時間（ミリ秒）をはさんで順に送る。 |
| `cyd_input_read` | Observation | なし | 直近の入力イベント（`tap.home`、`tap.<ボタン id>` など）を、消さずに返す。 |
| `cyd_input_clear` | Action | なし | 直近の入力イベントを返し、消す。 |
| `cyd_screen_mirror` | Observation | なし | UI ツリーから 320×240 の PNG を描いて返す。LCD の画素を読んだものではなく、キャンバスの図形、ゲーム、JPEG、ネイティブアプリの画面は写らない。 |
| `cyd_key_press` | Action | `key`: 文字列、`^(?:(?:CTRL|ALT|SHIFT)\+)*(?:[\x20-\x7e]|ENTER|BACKSPACE|DELETE|TAB|ESC|UP|DOWN|LEFT|RIGHT|HOME|END|PAGEUP|PAGEDOWN|INSERT|F(?:[1-9]|1[0-2]))$` | ハードウェアキーボードの 1 キーとして押す。エディタは編集に使い、実行中のアプリは cyd.key() で受け取る。名前は文字そのもの（a、A）、ENTER、BACKSPACE、DELETE、TAB、ESC、UP、DOWN、LEFT、RIGHT、HOME、END、PAGEUP、PAGEDOWN、INSERT、F1〜F12 で、前に CTRL+・ALT+・SHIFT+ を付けられる（CTRL+S）。CTRL+C は実行中のアプリを中断する。 |
| `cyd_type_text` | Action | `text`: 文字列、1〜512 文字、`^[\x20-\x7e\n]*$` | 文字列をハードウェアキーボードの打鍵として 1 文字ずつ送る。改行は ENTER。表示可能な ASCII だけ。 |

## L1 アプリ・設定・ファイル

| ツール | 種別 | 引数 | 説明 |
| --- | --- | --- | --- |
| `cyd_apps_list` | Observation | なし | 組み込みの画面とネイティブアプリの id を返す（ファームウェアに固定で書かれた一覧）。SD のアプリは `cyd_installed_apps_list`。 |
| `cyd_installed_apps_list` | Observation | なし | microSD にインストールされた MicroPython アプリと、Home に固定されているかを返す。 |
| `cyd_launch` | Action | `app_id`: 文字列 | 組み込みの画面（clock、calendar、scripts、settings、editor）、ネイティブアプリ、インストール済みの MicroPython アプリを id で開く。MicroPython アプリは起動をキューに入れるだけなので、成功は「起動を受け付けた」の意味。 |
| `cyd_activate` | Action | `app_id`: 文字列 | `cyd_launch` と同じ。古い名前で書かれたクライアント向け。 |
| `cyd_home` | Action | なし | 前面のアプリを終了して Home に戻る。 |
| `cyd_settings_get` | Observation | なし | 明るさ、テーマ、アニメーション、Bluetooth キーボードのキー配列の設定を返す。 |
| `cyd_settings_set` | Action | `brightness`: 文字列、`"dim"` / `"balanced"` / `"bright"`、省略可<br>`theme`: 文字列、`"blue"` / `"violet"` / `"teal"`、省略可<br>`animation`: 文字列、`"off"` / `"fast"` / `"smooth"`、省略可<br>`keyboard_layout`: 文字列、`"jis"` / `"us"`、省略可 | 明るさ、テーマ、アニメーション、Bluetooth キーボードのキー配列を変更して保存する。指定しなかった項目は変わらない。 |
| `cyd_package_upload` | Action | `app_id`: 文字列、`^[A-Za-z0-9_.-]{1,24}$`<br>`file`: 文字列、`^[A-Za-z0-9_.-]{1,24}$`<br>`content`: 文字列、最大 65536 文字<br>`encoding`: 文字列、`"utf8"` / `"base64"`、既定 `"utf8"`<br>`replace`: 真偽値、既定 `false` | ファイル 1 つを `/sd/apps/<app_id>/` へ SHA-256 を確かめながら書き込む。バイナリは `encoding: "base64"`。`replace` が false なら既存のファイルは上書きしない。アプリの実行中は失敗する。 |
| `cyd_app_deploy` | Action | `app_dir`: 文字列、1〜256 文字<br>`pin`: 真偽値、既定 `false`<br>`launch`: 真偽値、既定 `false`<br>`replace`: 真偽値、既定 `true` | アプリのディレクトリ（manifest.json、起動ファイル、画像などの全ファイル）を、manifest を検査したうえで転送する。`app_dir` は `CYD_DESKTOP_APPS_ROOT`（既定はリポジトリの `examples`）の中だけ。転送前に Home へ戻る。 |
| `cyd_script_launch` | Action | `app_id`: 文字列、`^[A-Za-z0-9_.-]{1,24}$` | `/sd/apps/<app_id>/` の MicroPython アプリを前面で起動する。 |
| `cyd_app_delete` | Action | `app_id`: 文字列、`^[A-Za-z0-9_.-]{1,24}$` | インストール済みの MicroPython アプリと、それを指す Home のショートカットを削除する。元に戻せない。 |
| `cyd_shortcuts_list` | Observation | なし | Home に表示している microSD 上のショートカットを返す。 |
| `cyd_shortcut_create` | Action | `shortcut_id`: 文字列、`^[A-Za-z0-9_.-]{1,24}$`<br>`app_id`: 文字列、`^[A-Za-z0-9_.-]{1,24}$`<br>`title`: 文字列、`^[A-Za-z0-9 _.-]{1,12}$` | Home のショートカットを作る。Home の 1 ページ目に 2 つ、以降のページに 8 つずつ、最大 18 個。 |
| `cyd_shortcut_update` | Action | `shortcut_id`: 文字列、`^[A-Za-z0-9_.-]{1,24}$`<br>`app_id`: 文字列、`^[A-Za-z0-9_.-]{1,24}$`<br>`title`: 文字列、`^[A-Za-z0-9 _.-]{1,12}$` | 既存のショートカットを変更する。 |
| `cyd_shortcut_delete` | Action | `shortcut_id`: 文字列、`^[A-Za-z0-9_.-]{1,24}$` | ショートカットを削除する。 |
| `cyd_time_set` | Action | `epoch`: 整数、946684800 以上 | Unix 時刻（秒）で本体の時計を合わせる。 |

## L2 ログと診断

| ツール | 種別 | 引数 | 説明 |
| --- | --- | --- | --- |
| `cyd_diagnostics` | Observation | `fingerprint`: 整数、0〜1、省略可 | 空きヒープ、タスク数、描画回数、SPI 転送量、Watchdog の状態、ネイティブアプリのフレーム時間などを返す。`fingerprint: 1` で LCD へ送った内容の指紋（CRC-32 の和）を 0 から数え始め、`0` で止める。 |
| `cyd_logs` | Observation | なし | 直近 16 件の UI・入力・アプリのイベントログを返す。 |
| `cyd_app_error` | Observation | なし | 直近に失敗した MicroPython アプリのトレースバック全文を返す（イベントログは 48 文字の要約だけ）。 |

## L3 ハードウェア

| ツール | 種別 | 引数 | 説明 |
| --- | --- | --- | --- |
| `cyd_touch_calibrate_start` | Action | なし | タッチ較正を始める。表示される 3 つの十字を順にタップすると保存される。 |
| `cyd_radio_status` | Observation | なし | この起動の無線モード（off、wifi、bluetooth）と、次回起動用に保存されたモードを返す。Wi-Fi と Bluetooth は排他。 |
| `cyd_radio_set` | Action | `mode`: 文字列、`"off"` / `"wifi"` / `"bluetooth"`<br>`restart`: 真偽値、既定 `false` | 次回起動の無線モード（off、wifi、bluetooth）を保存する。再起動後に有効。restart を true にすると応答の直後に再起動する（前面アプリの実行中は失敗）。 |
| `cyd_bt_status` | Observation | なし | Bluetooth キーボードの状態を返す。接続中と記憶済みのキーボード、ペアリングの進行と入力すべき PIN またはパスキー、直前のスキャン結果。Bluetooth は無線モードが bluetooth のときだけ動く。 |
| `cyd_bt_scan` | Action | `seconds`: 整数、1〜30、既定 `10` | ペアリングモードの Bluetooth Classic キーボードを指定秒数だけ探す。結果は cyd_bt_status で確認する。 |
| `cyd_bt_connect` | Action | `index`: 整数、0〜5 | 直前のスキャン結果の index のキーボードとペアリングする。画面（と cyd_bt_status）に出る PIN またはパスキーをキーボードで入力して Enter。新しくペアリングすると前のキーボードは忘れる。 |
| `cyd_bt_forget` | Action | なし | 記憶している Bluetooth キーボードを切断して忘れる。 |
| `cyd_sd_status` | Observation | なし | 起動時に microSD を安全にマウントできたかを返す。 |
