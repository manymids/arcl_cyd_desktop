#include "screen/keyboard.h"
#include "screen/draw.h"

#include <cstring>

namespace cyd::desktop::screen::keyboard {

void draw(uint8_t mode, int pressed_key) {
    line(0, 115, kWidth, 115, 0x4208, 1);
    fill_rect(0, 116, kWidth, 96, 0x1082);

    for (int k = 0; k < kKeyCount; ++k) {
        const KeyRect key = key_rect(k);
        if (tile_raster_active &&
            (key.y + key.height < tile_raster_y || key.y >= tile_raster_y + tile_raster_height))
            continue;

        const char *lbl = key_label(mode, k);
        const bool is_pressed = (pressed_key == k);
        const bool is_cap_active = (k == 22 && mode == kUpper);
        const bool is_sym_active = (k == 33 && mode == kSymbols);

        uint16_t bg = 0x2104;
        uint16_t fg = kWhite;
        if (is_pressed) {
            bg = kCyan;
            fg = kBlack;
        } else if (is_cap_active || is_sym_active) {
            bg = 0x03E0;
            fg = kWhite;
        } else if (k == 10 || k == 22 || k == 33 || k == 44 || k == 50) {
            bg = 0x2945;
        }

        fill_rect(key.x, key.y, key.width, key.height, bg);
        frame(key.x, key.y, key.width, key.height, is_pressed ? kWhite : 0x39E7);

        const int text_w = static_cast<int>(std::strlen(lbl)) * 6;
        text(lbl, key.x + (key.width - text_w) / 2, key.y + (key.height - 7) / 2, fg);
    }
}

}  // namespace cyd::desktop::screen::keyboard
