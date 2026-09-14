// cyd.tile_begin() / cyd.tile_present(): puts the tile map scene of
// tile_engine.cpp on the panel. Frames are rendered on the MicroPython task
// that presents them, under the display lock, like cyd.video_present().

#include "screen/display.h"
#include "screen/draw.h"
#include "native_app.h"
#include "shell_state.h"
#include "tile_engine.h"

extern "C" {
#include "driver/gpio.h"
}

#include <algorithm>

extern "C" void cyd_desktop_ui_refresh(void);

using namespace cyd::desktop::screen;
namespace native = cyd::desktop::native;
namespace tiles = cyd::desktop::tiles;

namespace {

bool take_display() {
    return display_mutex != nullptr && xSemaphoreTake(display_mutex, pdMS_TO_TICKS(1000)) == pdTRUE;
}

bool tile_view_in_front() {
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    return snapshot.view == cyd::desktop::View::Native &&
           native::has_flag(snapshot.foreground_app, native::kAppTileSurface);
}

}  // namespace

// Returns a tiles::Result, or -5 when there is no tile surface to enter and
// -6 when the display stayed busy.
extern "C" int cyd_desktop_tile_begin(const uint8_t *palette, size_t palette_size,
                                      const uint8_t *tile_data, size_t tiles_size,
                                      const uint8_t *sprite_data, size_t sprites_size,
                                      const uint8_t *map, size_t map_size, int map_width, int map_height,
                                      int origin_y) {
    const auto *surface = native::find_with_flag(native::kAppTileSurface);
    if (surface == nullptr) return -5;
    if (!take_display()) return -6;
    const int result = tiles::begin(palette, palette_size, tile_data, tiles_size, sprite_data, sprites_size,
                                    map, map_size, map_width, map_height, origin_y);
    xSemaphoreGive(display_mutex);
    if (result != tiles::kOk) return result;
    if (!tile_view_in_front()) {
        cyd::desktop::shell_state().launch_native(surface->id);
        cyd_desktop_ui_refresh();
    }
    return tiles::kOk;
}

extern "C" void cyd_desktop_tile_end(void) {
    if (!take_display()) {
        // The pointers must go before Python may collect the buffers.
        tiles::end();
        return;
    }
    tiles::end();
    xSemaphoreGive(display_mutex);
}

extern "C" int cyd_desktop_tile_palette(const uint8_t *palette, size_t size) {
    if (!take_display()) return -6;
    const int result = tiles::set_palette(palette, size);
    xSemaphoreGive(display_mutex);
    return result;
}

extern "C" void cyd_desktop_tile_frame(int scroll_x) { tiles::frame(scroll_x); }

extern "C" bool cyd_desktop_tile_sprite(int cell, int x, int y, int flags) {
    return tiles::sprite(cell, x, y, static_cast<uint8_t>(flags));
}

extern "C" bool cyd_desktop_tile_text(int slot, int x, int y, const char *value, int color, int scale) {
    return tiles::text(slot, x, y, value, static_cast<uint8_t>(color), scale);
}

// 0 when shown; -4 when the tile view is no longer in front (the app should
// stop), -6 when the display stayed busy.
extern "C" int cyd_desktop_tile_present(void) {
    if (!tile_view_in_front()) return -4;
    if (!take_display()) return -6;
    const auto *running = native::current();
    if (!tiles::ready() || running == nullptr || !(running->flags & native::kAppTileSurface)) {
        xSemaphoreGive(display_mutex);
        return -4;
    }
    const TickType_t started = xTaskGetTickCount();
    tiles::commit();
    clear_clip();
    for (int y = 0; y < kHeight; y += kTransitionTileRows) {
        const int height = std::min(kTransitionTileRows, kHeight - y);
        const native::TileSurface band{line_buffer, kWidth, height, kWidth, y};
        tiles::render(band);
        set_window(0, y, kWidth, height);
        gpio_set_level(static_cast<gpio_num_t>(kDisplayDc), 1);
        transmit(line_buffer, static_cast<size_t>(kWidth) * height * sizeof(uint16_t));
    }
    xSemaphoreGive(display_mutex);
    native::note_frame(static_cast<uint32_t>(xTaskGetTickCount() - started) * portTICK_PERIOD_MS);
    return 0;
}
