# P13: ARCLインターフェースの拡張

[English](p13-arcl-interface_en.md) | 日本語

この段階では、フレームバッファやPSRAMに依存せず、提案された7段階の操作・観測機能を実装しました。

> 当時の MCP ツール名 `desktop_*` は、ARCL 共通仕様の命名規約に合わせて `desktop_status` → `arcl_status`、それ以外 → `cyd_*` に変わっています。ファームウェアのプロトコルのコマンド名は `desktop_*` のままです。

## インターフェース

1. `desktop_tap` は画面の絶対座標によるタップをキューに入れます。bridgeには、待機時間を制限して最大64回のタップを順番に送る `desktop_input_macro` と、物理入力・キュー内入力を観測する `desktop_input_state` もあります。
2. `desktop_ui_tree` は、安定したid、role、label、bounds、enabled状態を持つ保持済みの意味的ノードを返します。デバイスのJSONL応答は1ページ4ノードまでで、MCP bridgeが全ページを結合します。構築用／公開用の二重バッファにより、再構築途中のツリーがクライアントから見えないようにしています。
3. `desktop_installed_apps_list`、`desktop_script_launch`、ショートカット操作、`desktop_app_delete`、`desktop_settings_get/set` により、ネイティブのScriptsとSettingsの機能を操作できます。設定は既存のNVS保存方式を使い、3段階の明るさ、テーマ、アニメーション値を扱います。
4. `desktop_screen_mirror` は、保持された意味的ツリーと共有の5×7グリフデータから、PC上で320×240のPNGを生成します。ILI9341のGRAMを読み戻すものではなく、論理的なミラーです。
5. `desktop_logs` は16件のイベントリングを読みます。拡張した診断には、現在／最小空きヒープ、FreeRTOSタスク数、論理描画回数、SPI転送回数・バイト数、合成タップ数、Watchdog状態、ランタイムのフレーム状態が含まれます。
6. `desktop_pause`、`desktop_step`、`desktop_run(frames)`、`desktop_resume` は、`cyd.update()` の境界でMicroPythonの協調実行を制御します。一時停止中の実時間は5分間の実行上限に含めず、その間もネイティブWatchdogに生存を通知します。
7. MicroPythonアプリは `cyd.checkpoint(value)` でJSON互換の状態を保存し、次回起動時に `cyd.restore(default)` で復元し、`cyd.clear_checkpoint()` で削除できます。ペイロードは4096バイトまでで、置換前に一時ファイルを使用します。

物理タッチと合成タップは同じネイティブのヒットテストに合流します。既存のHome長押しによる脱出とWatchdogによる復帰が引き続き有効です。

## リソースと実機検証

この段階の最終ESP32アプリイメージは768,912バイトで、最小アプリパーティションに1,262,704バイト（62%）の空きがあります。ELFのBSSは20,393バイトです。全実機テスト終了時の空きヒープは190,996バイト、記録された最小空きヒープは190,880バイトでした。書き込んだアプリイメージのSHA-256は `0e82db97dace2571ce2c719e4bc7e68b9a8b41dcae6d0002bd474eed96876d83` です。

2026-09-10に `npm test`、通常のCOM9統合テスト、`npm run integration:p13` が成功しました。P13の実行では、Homeの7ノード、Settingsへの合成タップ、設定値を変えない読書き、ページ分割された7アプリの検出、2回の起動をまたぐcheckpoint、使い捨てテストパッケージの削除、安定した協調pause、正確な1step、2フレームの限定実行、resume、Homeへの復帰を確認しました。アプリパーティションのみの書込みにより、既存のNVS較正・設定とmicroSD上のアプリを維持しました。
