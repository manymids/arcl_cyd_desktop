# Desktop shell core

English | [日本語](README_ja.md)

The C++ state machine owns Home, Start and exactly one foreground app.  It is
deliberately independent of LCD, touch and MicroPython so those adapters can
be tested independently and no app can create a second window.

Views live in `screen_*.cpp`, the JSON Lines protocol in `protocol.cpp` and the
MicroPython binding in `shell_module.cpp`. Radio mode and the Bluetooth keyboard
are in `bt_keyboard.cpp` (reached through `include/bt_keyboard.h`), layout
translation in `key_input.cpp` and key routing in `screen_keys.cpp`. Host tests
are in `tests/`; run them with `tools/run-native-tests.sh`.
