// cyd.sprite_present(): puts the sprite scene of scene_engine.cpp on the
// panel. Like the tile map, frames are rendered on the MicroPython task that
// presents them, under the display lock, 16 rows at a time.

#include "screen/display.h"
#include "screen/draw.h"
#include "native_app.h"
#include "scene_engine.h"
#include "n3d_engine.h"
#include "shell_state.h"

extern "C" {
#include "driver/gpio.h"
}

#include <algorithm>

extern "C" void cyd_desktop_ui_refresh(void);

using namespace cyd::desktop::screen;
namespace native = cyd::desktop::native;
namespace scene = cyd::desktop::scene;

namespace {

bool take_display() {
    return display_mutex != nullptr && xSemaphoreTake(display_mutex, pdMS_TO_TICKS(1000)) == pdTRUE;
}

bool scene_view_in_front() {
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    return snapshot.view == cyd::desktop::View::Native &&
           native::has_flag(snapshot.foreground_app, native::kAppSceneSurface);
}

}  // namespace

// Allocates the scene and brings its view to the front. 0 on success, -1 when
// the frame does not fit in memory, -5 without a scene surface, -6 when the
// display stayed busy.
extern "C" int cyd_desktop_scene_begin(void) {
    const auto *surface = native::find_with_flag(native::kAppSceneSurface);
    if (surface == nullptr) return -5;
    if (!take_display()) return -6;
    const bool allocated = scene::begin();
    xSemaphoreGive(display_mutex);
    if (!allocated) return -1;
    if (!scene_view_in_front()) {
        cyd::desktop::shell_state().launch_native(surface->id);
        cyd_desktop_ui_refresh();
    }
    return 0;
}

extern "C" void cyd_desktop_scene_end(void) {
    // The pointers must go before Python may collect the buffers, so drop them
    // even if the display stays busy.
    const bool locked = take_display();
    scene::end();
    if (locked) xSemaphoreGive(display_mutex);
}

extern "C" bool cyd_desktop_scene_image(int slot, const uint16_t *pixels, int width, int height) {
    const bool locked = take_display();
    const bool result = scene::set_image(slot, pixels, width, height);
    if (locked) xSemaphoreGive(display_mutex);
    return result;
}

extern "C" void cyd_desktop_scene_overlay_clear(void) { scene::overlay_clear(); }

extern "C" bool cyd_desktop_scene_overlay_add(int kind, int x, int y, int width, int height, int color,
                                              const char *value, int scale, int shadow, int align) {
    return scene::overlay_add(kind, x, y, width, height, static_cast<uint16_t>(color), value, scale,
                              static_cast<uint16_t>(shadow), align);
}

namespace {

void send_frame() {
    clear_clip();
    for (int y = 0; y < kHeight; y += kTransitionTileRows) {
        const int height = std::min(kTransitionTileRows, kHeight - y);
        const native::TileSurface band{line_buffer, kWidth, height, kWidth, y};
        scene::render(band);
        set_window(0, y, kWidth, height);
        gpio_set_level(static_cast<gpio_num_t>(kDisplayDc), 1);
        transmit(line_buffer, static_cast<size_t>(kWidth) * height * sizeof(uint16_t));
    }
}

}  // namespace

// The record count when shown; scene::kBadRecords (-2) for an invalid record,
// -4 when the scene view is no longer in front (the app should stop), -6 when
// the display stayed busy.
extern "C" int cyd_desktop_scene_present(const uint8_t *records, size_t size, int background) {
    if (!scene_view_in_front()) return -4;
    if (!take_display()) return -6;
    const TickType_t started = xTaskGetTickCount();
    const int result = scene::prepare(records, size, static_cast<uint16_t>(background));
    if (result < 0) {
        xSemaphoreGive(display_mutex);
        return result == scene::kNotReady ? -4 : result;
    }
    send_frame();
    xSemaphoreGive(display_mutex);
    native::note_frame(static_cast<uint32_t>(xTaskGetTickCount() - started) * portTICK_PERIOD_MS);
    return result;
}

// n3d_present(): the items' polygons under the sprite records, with the
// overlay on top. The polygon count when shown; -1 without memory for the 3D
// buffers, -2 for an invalid sprite record, -4 when the scene view is no
// longer in front, -6 when the display stayed busy.
extern "C" int cyd_desktop_scene_present_3d(const uint8_t *items, size_t item_count, const uint8_t *records,
                                            size_t records_size, int background, bool z_buffer) {
    if (!scene_view_in_front()) return -4;
    if (!scene::valid_records(records, records_size)) return scene::kBadRecords;
    if (!take_display()) return -6;
    const TickType_t started = xTaskGetTickCount();
    if (!scene::begin_3d()) {
        xSemaphoreGive(display_mutex);
        return -1;
    }
    // Building under the display lock keeps a UI-task redraw from seeing a
    // half-written polygon list.
    int polygons = 0;
    for (size_t index = 0; index < item_count && polygons < scene::kPolyCapacity; ++index)
        polygons += cyd::desktop::n3d::build_item(items + index * cyd::desktop::n3d::kItemBytes, false,
                                                  scene::polys() + polygons, scene::kPolyCapacity - polygons);
    const int result = scene::prepare(records, records_size, static_cast<uint16_t>(background), polygons, z_buffer);
    if (result < 0) {
        xSemaphoreGive(display_mutex);
        return result == scene::kNotReady ? -4 : result;
    }
    send_frame();
    xSemaphoreGive(display_mutex);
    native::note_frame(static_cast<uint32_t>(xTaskGetTickCount() - started) * portTICK_PERIOD_MS);
    return polygons;
}
