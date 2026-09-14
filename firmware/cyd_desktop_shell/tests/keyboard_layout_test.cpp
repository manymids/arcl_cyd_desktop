// The on-screen keyboard used to be drawn by three copies of the same loop
// (console, editor, Save As) and hit-tested by two more. A change to one copy
// could make a tap select a different key from the one drawn under the finger.
// Now there is one layout; this checks that drawing and hit testing agree.

#include <cassert>
#include <cstdio>
#include <cstring>

#include "screen/keyboard.h"

namespace kb = cyd::desktop::screen::keyboard;

namespace {

void every_pixel_of_a_key_selects_that_key() {
    for (int index = 0; index < kb::kKeyCount; ++index) {
        const kb::KeyRect key = kb::key_rect(index);
        for (int y = key.y; y < key.y + key.height; ++y) {
            for (int x = key.x; x < key.x + key.width; ++x) {
                if (kb::key_at(x, y) != index) {
                    std::printf("key %d drawn at (%d,%d) but a tap there selects %d\n",
                                index, x, y, kb::key_at(x, y));
                    assert(false);
                }
            }
        }
    }
}

void keys_do_not_overlap_and_stay_above_the_taskbar() {
    for (int a = 0; a < kb::kKeyCount; ++a) {
        const kb::KeyRect ka = kb::key_rect(a);
        assert(ka.x >= 0 && ka.x + ka.width <= 320);
        assert(ka.y >= 116 && ka.y + ka.height <= 212);
        for (int b = a + 1; b < kb::kKeyCount; ++b) {
            const kb::KeyRect kb_ = kb::key_rect(b);
            const bool apart = ka.x + ka.width <= kb_.x || kb_.x + kb_.width <= ka.x ||
                               ka.y + ka.height <= kb_.y || kb_.y + kb_.height <= ka.y;
            assert(apart);
        }
    }
}

void taps_outside_the_keyboard_select_nothing() {
    assert(kb::key_at(100, 116) == -1);
    assert(kb::key_at(100, 212) == -1);
    assert(kb::key_at(100, 0) == -1);
    // cyd.console_key_at() passes touch coordinates straight through, and
    // cyd.touch() reports -1 before the first touch.
    assert(kb::key_at(-1, 125) == -1);
    assert(kb::key_at(320, 125) == -1);
    // Gaps between keys still select the nearest key, as before.
    assert(kb::key_at(30, 125) == 0);
    assert(kb::key_at(0, 125) == 0);
    assert(kb::key_at(319, 125) == 10);
}

void control_keys_are_where_the_editor_expects_them() {
    for (uint8_t mode = kb::kLower; mode <= kb::kSymbols; ++mode) {
        assert(kb::key_char(mode, 10) == kb::kBackspace);
        assert(kb::key_char(mode, 22) == kb::kCaps);
        assert(kb::key_char(mode, 33) == kb::kSymbolShift);
        assert(kb::key_char(mode, 44) == kb::kEscape);
        assert(kb::key_char(mode, 45) == ' ');
        assert(kb::key_char(mode, 50) == kb::kEnter);
        assert(std::strcmp(kb::key_label(mode, 50), "ENTER") == 0);
    }
    assert(kb::key_char(kb::kLower, 11) == 'q');
    assert(kb::key_char(kb::kUpper, 11) == 'Q');
    assert(kb::key_char(kb::kSymbols, 11) == '<');
    // An unknown mode reads as lower case rather than out of bounds.
    assert(kb::key_char(7, 11) == 'q');
}

void labels_and_characters_agree() {
    for (uint8_t mode = kb::kLower; mode <= kb::kSymbols; ++mode) {
        for (int index = 0; index < kb::kKeyCount; ++index) {
            const char *label = kb::key_label(mode, index);
            const char ch = kb::key_char(mode, index);
            if (std::strlen(label) == 1) assert(label[0] == ch);
        }
    }
}

}  // namespace

int main() {
    every_pixel_of_a_key_selects_that_key();
    keys_do_not_overlap_and_stay_above_the_taskbar();
    taps_outside_the_keyboard_select_nothing();
    control_keys_are_where_the_editor_expects_them();
    labels_and_characters_agree();
    std::puts("keyboard_layout_test: all assertions passed");
    return 0;
}
