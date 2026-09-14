#pragma once

// The 51-key on-screen keyboard shared by the editor, the editor's Save As
// prompt and the MicroPython console. It occupies y 115..211, above the
// taskbar.
//
// Layout and hit testing (keyboard_layout.cpp) are pure and host-tested:
// the key a tap selects is always the key drawn under it. Drawing
// (screen_keyboard.cpp) needs the LCD.
//
// MicroPython apps reach the same layout through cyd.console_key_at() and
// cyd.console_key_char(), so nothing outside this file copies it.

#include <cstdint>

namespace cyd::desktop::screen::keyboard {

constexpr int kKeyCount = 51;

enum Mode : uint8_t { kLower = 0, kUpper = 1, kSymbols = 2 };

// key_char() returns these for the keys that do not insert text.
constexpr char kBackspace = '\x08';
constexpr char kCaps = '\x01';
constexpr char kSymbolShift = '\x02';
constexpr char kEscape = '\x1b';
constexpr char kEnter = '\n';

struct KeyRect { int x; int y; int width; int height; };

KeyRect key_rect(int index);
// The key under (x, y), or -1 outside the keyboard.
int key_at(int x, int y);
// Unknown modes read as kLower.
const char *key_label(uint8_t mode, int index);
char key_char(uint8_t mode, int index);

// Separator, background and all keys; `pressed_key` (or -1) is highlighted.
// Inside a transition tile, keys outside the tile are skipped.
void draw(uint8_t mode, int pressed_key);

}  // namespace cyd::desktop::screen::keyboard
