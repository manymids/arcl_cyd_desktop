# Desktopシェルのコア

[English](README_en.md) | 日本語

C++の状態機械がHome、Start、単一の前景アプリを管理します。
LCD、タッチ、MicroPythonから意図的に独立させることで、各アダプターを個別にテストでき、
アプリが2つ目のウィンドウを作れない構成にしています。

画面は `screen_*.cpp`（画面ごと）、JSON Linesプロトコルは `protocol.cpp`、MicroPythonとの接点は `shell_module.cpp` です。
無線モードとBluetoothキーボードは `bt_keyboard.cpp`（`include/bt_keyboard.h` 経由）、キー配列の変換は `key_input.cpp`、
キーの振り分けは `screen_keys.cpp` にあります。ホストで動くテストは `tests/` にあり、`tools/run-native-tests.sh` で実行します。
