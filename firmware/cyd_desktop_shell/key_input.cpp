#include "key_input.h"

#include <cstdio>
#include <cstring>

namespace cyd::desktop::keys {

namespace {

constexpr uint8_t kHidCtrl = 0x11;
constexpr uint8_t kHidShift = 0x22;
constexpr uint8_t kHidAlt = 0x44;

// Usages 0x2d..0x38. '\x1b' stands for Escape: on JIS keyboards the
// Hankaku/Zenkaku key sits where Escape is expected on compact boards.
constexpr char kUsLower[] = "-=[]\\#;'`,./";
constexpr char kUsUpper[] = "_+{}|~:\"~<>?";
constexpr char kJisLower[] = "-^@[]];:\x1b,./";
constexpr char kJisUpper[] = "=~`{}}+*\x1b<>?";

constexpr const char kUsDigitsShifted[] = "!@#$%^&*()";
// JIS Shift+0 types nothing.
constexpr const char kJisDigitsShifted[] = "!\"#$%&'()\0";

struct SpecialName {
    uint16_t code;
    const char *name;
};

constexpr SpecialName kSpecialNames[] = {
    {kEnter, "ENTER"}, {kBackspace, "BACKSPACE"}, {kDelete, "DELETE"}, {kTab, "TAB"},
    {kEscape, "ESC"}, {kUp, "UP"}, {kDown, "DOWN"}, {kLeft, "LEFT"}, {kRight, "RIGHT"},
    {kHome, "HOME"}, {kEnd, "END"}, {kPageUp, "PAGEUP"}, {kPageDown, "PAGEDOWN"},
    {kInsert, "INSERT"}, {kCapsLock, "CAPSLOCK"},
};

Key character(char value, uint8_t hid_modifiers, bool shifted_form) {
    Key key{static_cast<uint16_t>(static_cast<unsigned char>(value)), 0};
    if (hid_modifiers & kHidCtrl) key.modifiers |= kCtrl;
    if (hid_modifiers & kHidAlt) key.modifiers |= kAlt;
    // Shift already chose the character; it is reported only alongside Ctrl or Alt.
    if (key.modifiers != 0 && shifted_form) key.modifiers |= kShift;
    return key;
}

Key special(uint16_t code, uint8_t hid_modifiers) {
    Key key{code, 0};
    if (hid_modifiers & kHidCtrl) key.modifiers |= kCtrl;
    if (hid_modifiers & kHidShift) key.modifiers |= kShift;
    if (hid_modifiers & kHidAlt) key.modifiers |= kAlt;
    return key;
}

}  // namespace

Key translate(uint8_t usage, uint8_t hid_modifiers, Layout layout, bool caps_lock) {
    const bool shift = (hid_modifiers & kHidShift) != 0;
    const bool command = (hid_modifiers & (kHidCtrl | kHidAlt)) != 0;
    if (usage >= 0x04 && usage <= 0x1d) {
        // With Ctrl or Alt the letter is reported in lower case plus kShift.
        const bool upper = !command && (shift != caps_lock);
        return character(static_cast<char>((upper ? 'A' : 'a') + (usage - 0x04)), hid_modifiers, shift);
    }
    if (usage >= 0x1e && usage <= 0x27) {
        const int index = usage - 0x1e;  // 1..9, 0
        if (!shift || command) return character(static_cast<char>(index == 9 ? '0' : '1' + index), hid_modifiers, shift);
        const char value = (layout == kLayoutUs ? kUsDigitsShifted : kJisDigitsShifted)[index];
        return value == '\0' ? Key{kNone, 0} : character(value, hid_modifiers, true);
    }
    if (usage >= 0x2d && usage <= 0x38) {
        const int index = usage - 0x2d;
        const bool jis = layout == kLayoutJis;
        const char value = shift && !command ? (jis ? kJisUpper : kUsUpper)[index]
                                             : (jis ? kJisLower : kUsLower)[index];
        if (value == '\x1b') return special(kEscape, hid_modifiers);
        return character(value, hid_modifiers, shift);
    }
    switch (usage) {
        case 0x28: case 0x58: return special(kEnter, hid_modifiers);
        case 0x29: return special(kEscape, hid_modifiers);
        case 0x2a: return special(kBackspace, hid_modifiers);
        case 0x2b: return special(kTab, hid_modifiers);
        case 0x2c: return character(' ', hid_modifiers, shift);
        case 0x39: return special(kCapsLock, hid_modifiers);
        case 0x49: return special(kInsert, hid_modifiers);
        case 0x4a: return special(kHome, hid_modifiers);
        case 0x4b: return special(kPageUp, hid_modifiers);
        case 0x4c: return special(kDelete, hid_modifiers);
        case 0x4d: return special(kEnd, hid_modifiers);
        case 0x4e: return special(kPageDown, hid_modifiers);
        case 0x4f: return special(kRight, hid_modifiers);
        case 0x50: return special(kLeft, hid_modifiers);
        case 0x51: return special(kDown, hid_modifiers);
        case 0x52: return special(kUp, hid_modifiers);
        case 0x54: return character('/', hid_modifiers, false);
        case 0x55: return character('*', hid_modifiers, false);
        case 0x56: return character('-', hid_modifiers, false);
        case 0x57: return character('+', hid_modifiers, false);
        case 0x62: return character('0', hid_modifiers, false);
        case 0x63: return character('.', hid_modifiers, false);
        // Non-US backslash (ISO), JIS Ro and JIS Yen.
        case 0x64: return character(shift && !command ? '|' : '\\', hid_modifiers, shift);
        case 0x87: return character(shift && !command ? '_' : '\\', hid_modifiers, shift);
        case 0x89: return character(shift && !command ? '|' : '\\', hid_modifiers, shift);
        default: break;
    }
    if (usage >= 0x59 && usage <= 0x61) return character(static_cast<char>('1' + (usage - 0x59)), hid_modifiers, false);
    if (usage >= 0x3a && usage <= 0x45) return special(static_cast<uint16_t>(kF1 + (usage - 0x3a)), hid_modifiers);
    return {kNone, 0};
}

bool name(const Key &key, char *out, size_t capacity) {
    if (capacity == 0 || key.code == kNone) return false;
    char base[12] = {};
    if (printable(key)) {
        char value = static_cast<char>(key.code);
        // Letters after a modifier are written in upper case: CTRL+S.
        if (key.modifiers & (kCtrl | kAlt)) {
            if (value >= 'a' && value <= 'z') value = static_cast<char>(value - 'a' + 'A');
        }
        base[0] = value;
    } else if (key.code >= kF1 && key.code < kF1 + 12) {
        std::snprintf(base, sizeof(base), "F%d", key.code - kF1 + 1);
    } else {
        for (const auto &entry : kSpecialNames) {
            if (entry.code == key.code) std::snprintf(base, sizeof(base), "%s", entry.name);
        }
        if (base[0] == '\0') return false;
    }
    const bool text = is_text(key);
    const int written = std::snprintf(out, capacity, "%s%s%s%s",
        (key.modifiers & kCtrl) ? "CTRL+" : "", (key.modifiers & kAlt) ? "ALT+" : "",
        (!text && (key.modifiers & kShift)) ? "SHIFT+" : "", base);
    return written > 0 && static_cast<size_t>(written) < capacity;
}

bool parse(const char *text, Key *out) {
    if (text == nullptr || text[0] == '\0') return false;
    Key key{kNone, 0};
    // A single character is itself, even "+".
    while (text[0] != '\0' && text[1] != '\0') {
        if (std::strncmp(text, "CTRL+", 5) == 0) { key.modifiers |= kCtrl; text += 5; }
        else if (std::strncmp(text, "ALT+", 4) == 0) { key.modifiers |= kAlt; text += 4; }
        else if (std::strncmp(text, "SHIFT+", 6) == 0) { key.modifiers |= kShift; text += 6; }
        else break;
    }
    if (text[0] == '\0') return false;
    if (text[1] == '\0') {
        char value = text[0];
        if (value < 32 || value > 126) return false;
        if (key.modifiers & (kCtrl | kAlt)) {
            if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
        } else {
            key.modifiers = 0;  // SHIFT+a is just "A" typed some other way
        }
        key.code = static_cast<uint16_t>(value);
        *out = key;
        return true;
    }
    if (text[0] == 'F' && text[1] >= '1' && text[1] <= '9') {
        int number = 0;
        if (std::sscanf(text + 1, "%d", &number) == 1 && number >= 1 && number <= 12) {
            char check[4];
            std::snprintf(check, sizeof(check), "%d", number);
            if (std::strcmp(check, text + 1) == 0) {
                key.code = static_cast<uint16_t>(kF1 + number - 1);
                *out = key;
                return true;
            }
        }
        return false;
    }
    for (const auto &entry : kSpecialNames) {
        if (std::strcmp(entry.name, text) == 0) {
            key.code = entry.code;
            *out = key;
            return true;
        }
    }
    return false;
}

}  // namespace cyd::desktop::keys
