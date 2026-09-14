#pragma once

// Keys from a hardware keyboard, independent of where they came from.
//
// A Bluetooth keyboard reports USB HID usages; translate() turns one into a
// Key using the chosen layout (JIS or US). Keys also arrive already translated
// from the JSON Lines protocol (desktop_key), which names them the way apps see
// them: "a", "A", "ENTER", "CTRL+S". name() and parse() convert between the two.
//
// Pure logic: host-tested by tests/key_input_test.cpp.

#include <cstddef>
#include <cstdint>

namespace cyd::desktop::keys {

enum Code : uint16_t {
    kNone = 0,
    // 32..126 are the printable ASCII characters themselves.
    kEnter = 0x100,
    kBackspace,
    kDelete,
    kTab,
    kEscape,
    kUp,
    kDown,
    kLeft,
    kRight,
    kHome,
    kEnd,
    kPageUp,
    kPageDown,
    kInsert,
    kCapsLock,
    kF1,  // kF1 + 11 is F12
};

enum Modifier : uint8_t {
    kCtrl = 1,
    kShift = 2,  // only with keys that are not characters, or with Ctrl/Alt
    kAlt = 4,
};

enum Layout : uint8_t { kLayoutJis = 0, kLayoutUs = 1 };

struct Key {
    uint16_t code;
    uint8_t modifiers;
};

inline bool printable(const Key &key) { return key.code >= 32 && key.code <= 126; }
// A character to insert as text: printable and without Ctrl or Alt.
inline bool is_text(const Key &key) { return printable(key) && (key.modifiers & (kCtrl | kAlt)) == 0; }
inline bool is_ctrl(const Key &key, char letter) {
    return key.code == static_cast<uint16_t>(letter) && (key.modifiers & (kCtrl | kAlt)) == kCtrl;
}

// `hid_modifiers` is the boot-protocol modifier byte. Returns code kNone for
// usages that type nothing (including modifier keys and JIS Shift+0).
Key translate(uint8_t usage, uint8_t hid_modifiers, Layout layout, bool caps_lock);

// "a", "ENTER", "CTRL+S", "SHIFT+TAB", "CTRL+ALT+DELETE". Returns false if
// `capacity` is too small or the key is kNone.
bool name(const Key &key, char *out, size_t capacity);
// The inverse of name(); letters after a modifier may be either case.
bool parse(const char *text, Key *out);

}  // namespace cyd::desktop::keys
