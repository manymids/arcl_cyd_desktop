# 公開ドキュメント

[English](README_en.md) | 日本語

利用者・アプリ開発者向けの資料をこのフォルダに残しています。
各文書を日本語版と英語版の対で掲載しています。

| 資料 | 用途 |
| --- | --- |
| [cyd API リファレンス](cyd-api_ja.md) | MicroPythonアプリの構成、manifest、インストール、UI・ゲーム・画像・コンソール・ハードウェアキーボード・保存・無線モードとネットワークAPI |
| [ネイティブアプリの追加手順](native-app-guide_ja.md) | C++アプリの実装、登録、ビルドへの追加、制約とテスト |
| [ビルド環境を一から作る](build_ja.md) | WSL、ESP-IDF v5.5.2 の導入、同梱した MicroPython v1.29.0（サブモジュール）の取得、初回ビルド、書き込み、テスト |
| [ビルド済みファームウェア](firmware-release_ja.md) | Releases の zip の中身、SHA-256 の確認、esptool での書き込み、`npm run package:firmware` によるリリースの作り方 |
| [ビルド環境の基線](p2-build-baseline_ja.md) | ESP-IDF／MicroPythonの基線バージョンとWSL構成の背景 |
| [MCP ツール一覧](mcp-tools_ja.md) | MCP bridge が公開する全ツールの種別・レイヤ・引数・説明（bridge から自動生成） |
| [ARCL 共通仕様 v0.5](external/arcl-common-spec_ja.md) | 別プロジェクトの外部仕様（参照用に同梱、MIT の対象外）。MCP ツールの命名とレイヤはこれに準拠 |
| [MCP操作・観測インターフェース](p13-arcl-interface_ja.md) | UIツリー、入力、診断、協調実行制御、checkpointの概要 |

接続設定、画像なしの最小アプリ、無線モードとBluetoothキーボードの使い方、現在の増分ビルド・書込み手順は
[プロジェクトREADME](../README_ja.md)を参照してください。

## 既存資料を読む際の注意

- `p2-build-baseline_ja.md` と `p13-arcl-interface_ja.md` は特定の開発段階の記録を
  兼ねています。ファームウェアサイズ、ヒープ使用量、ポート番号、検証結果は
  当時の値です。現在の製品仕様や性能保証として扱わないでください。
