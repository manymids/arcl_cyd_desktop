// Hardware keyboard translation: USB HID usages to keys for the JIS and US
// layouts, and the names apps and the protocol use for them.

#include <cassert>
#include <cstdio>
#include <cstring>

#include "key_input.h"

namespace k = cyd::desktop::keys;

namespace {

constexpr uint8_t kLeftShift = 0x02;
constexpr uint8_t kRightShift = 0x20;
constexpr uint8_t kLeftCtrl = 0x01;
constexpr uint8_t kLeftAlt = 0x04;

void expect_name(uint8_t usage, uint8_t modifiers, k::Layout layout, bool caps, const char *expected) {
    const k::Key key = k::translate(usage, modifiers, layout, caps);
    char text[24] = {};
    const bool named = k::name(key, text, sizeof(text));
    if (expected == nullptr) {
        if (named) {
            std::printf("usage 0x%02x mods 0x%02x layout %d: expected nothing, got \"%s\"\n", usage, modifiers, layout, text);
            assert(false);
        }
        return;
    }
    if (!named || std::strcmp(text, expected) != 0) {
        std::printf("usage 0x%02x mods 0x%02x layout %d caps %d: expected \"%s\", got \"%s\"\n",
                    usage, modifiers, layout, caps, expected, named ? text : "(none)");
        assert(false);
    }
}

void letters_follow_shift_and_caps_lock() {
    expect_name(0x04, 0, k::kLayoutJis, false, "a");
    expect_name(0x04, kLeftShift, k::kLayoutJis, false, "A");
    expect_name(0x1d, kRightShift, k::kLayoutUs, false, "Z");
    expect_name(0x04, 0, k::kLayoutUs, true, "A");
    expect_name(0x04, kLeftShift, k::kLayoutUs, true, "a");
}

void ctrl_and_alt_name_the_letter_in_upper_case() {
    expect_name(0x16, kLeftCtrl, k::kLayoutJis, false, "CTRL+S");
    expect_name(0x06, kLeftCtrl, k::kLayoutUs, true, "CTRL+C");
    expect_name(0x06, kLeftCtrl | kLeftShift, k::kLayoutUs, false, "CTRL+SHIFT+C");
    expect_name(0x1b, kLeftAlt, k::kLayoutJis, false, "ALT+X");
    const k::Key ctrl_c = k::translate(0x06, kLeftCtrl, k::kLayoutJis, false);
    assert(k::is_ctrl(ctrl_c, 'c'));
    assert(!k::is_text(ctrl_c));
    assert(!k::is_ctrl(k::translate(0x06, kLeftCtrl | kLeftAlt, k::kLayoutJis, false), 'c'));
}

void shifted_digits_differ_between_layouts() {
    expect_name(0x1f, kLeftShift, k::kLayoutUs, false, "@");
    expect_name(0x1f, kLeftShift, k::kLayoutJis, false, "\"");
    expect_name(0x24, kLeftShift, k::kLayoutUs, false, "&");
    expect_name(0x23, kLeftShift, k::kLayoutJis, false, "&");
    expect_name(0x24, kLeftShift, k::kLayoutJis, false, "'");
    expect_name(0x27, kLeftShift, k::kLayoutUs, false, ")");
    expect_name(0x27, kLeftShift, k::kLayoutJis, false, nullptr);
    expect_name(0x27, 0, k::kLayoutJis, false, "0");
}

void punctuation_differs_between_layouts() {
    // usage, US, US shifted, JIS, JIS shifted
    struct Row { uint8_t usage; const char *us; const char *us_shift; const char *jis; const char *jis_shift; };
    const Row rows[] = {
        {0x2d, "-", "_", "-", "="},
        {0x2e, "=", "+", "^", "~"},
        {0x2f, "[", "{", "@", "`"},
        {0x30, "]", "}", "[", "{"},
        {0x32, "#", "~", "]", "}"},
        {0x33, ";", ":", ";", "+"},
        {0x34, "'", "\"", ":", "*"},
        {0x35, "`", "~", "ESC", "SHIFT+ESC"},
        {0x36, ",", "<", ",", "<"},
        {0x37, ".", ">", ".", ">"},
        {0x38, "/", "?", "/", "?"},
        {0x87, "\\", "_", "\\", "_"},
        {0x89, "\\", "|", "\\", "|"},
    };
    for (const Row &row : rows) {
        expect_name(row.usage, 0, k::kLayoutUs, false, row.us);
        expect_name(row.usage, kLeftShift, k::kLayoutUs, false, row.us_shift);
        expect_name(row.usage, 0, k::kLayoutJis, false, row.jis);
        expect_name(row.usage, kLeftShift, k::kLayoutJis, false, row.jis_shift);
    }
}

void special_keys_carry_every_modifier() {
    expect_name(0x28, 0, k::kLayoutJis, false, "ENTER");
    expect_name(0x58, 0, k::kLayoutJis, false, "ENTER");
    expect_name(0x2a, 0, k::kLayoutJis, false, "BACKSPACE");
    expect_name(0x2b, kLeftShift, k::kLayoutJis, false, "SHIFT+TAB");
    expect_name(0x2c, 0, k::kLayoutJis, false, " ");
    expect_name(0x4c, kLeftCtrl | kLeftAlt, k::kLayoutUs, false, "CTRL+ALT+DELETE");
    expect_name(0x50, kLeftCtrl, k::kLayoutUs, false, "CTRL+LEFT");
    expect_name(0x52, 0, k::kLayoutUs, false, "UP");
    expect_name(0x3a, 0, k::kLayoutUs, false, "F1");
    expect_name(0x45, 0, k::kLayoutUs, false, "F12");
    expect_name(0x39, 0, k::kLayoutUs, false, "CAPSLOCK");
    expect_name(0x5b, 0, k::kLayoutUs, false, "3");
    expect_name(0x55, kLeftShift, k::kLayoutUs, false, "*");
    // Modifier keys themselves and unknown usages type nothing.
    expect_name(0xe1, kLeftShift, k::kLayoutUs, false, nullptr);
    expect_name(0x03, 0, k::kLayoutUs, false, nullptr);
}

void names_parse_back_to_the_same_key() {
    int checked = 0;
    for (int layout = 0; layout < 2; ++layout) {
        for (int usage = 0; usage < 0x100; ++usage) {
            const uint8_t modifier_sets[] = {0x00, 0x02, 0x01, 0x03, 0x04, 0x05};
            for (uint8_t modifiers : modifier_sets) {
                const k::Key key = k::translate(static_cast<uint8_t>(usage), modifiers,
                                                static_cast<k::Layout>(layout), false);
                char text[24];
                if (!k::name(key, text, sizeof(text))) continue;
                k::Key parsed{};
                if (!k::parse(text, &parsed) || parsed.code != key.code || parsed.modifiers != key.modifiers) {
                    std::printf("\"%s\" (code %u mods %u) parsed as code %u mods %u\n",
                                text, key.code, key.modifiers, parsed.code, parsed.modifiers);
                    assert(false);
                }
                ++checked;
            }
        }
    }
    assert(checked > 500);
}

void parse_accepts_what_people_type() {
    k::Key key{};
    assert(k::parse("+", &key) && key.code == '+' && key.modifiers == 0);
    assert(k::parse("CTRL+s", &key) && key.code == 's' && key.modifiers == k::kCtrl);
    assert(k::parse("CTRL++", &key) && key.code == '+' && key.modifiers == k::kCtrl);
    assert(k::parse("SHIFT+a", &key) && key.code == 'a' && key.modifiers == 0);
    assert(k::parse("F10", &key) && key.code == k::kF1 + 9);
    assert(!k::parse("F13", &key));
    assert(!k::parse("F01", &key));
    assert(!k::parse("", &key));
    assert(!k::parse("ENTERR", &key));
    assert(!k::parse("\t", &key));
    assert(!k::parse("ALT+", &key));
    assert(!k::parse("CTRL+ALT+", &key));
    char small[4];
    assert(!k::name({k::kBackspace, 0}, small, sizeof(small)));
}

}  // namespace

int main() {
    letters_follow_shift_and_caps_lock();
    ctrl_and_alt_name_the_letter_in_upper_case();
    shifted_digits_differ_between_layouts();
    punctuation_differs_between_layouts();
    special_keys_carry_every_modifier();
    names_parse_back_to_the_same_key();
    parse_accepts_what_people_type();
    std::puts("key_input_test: ok");
    return 0;
}
