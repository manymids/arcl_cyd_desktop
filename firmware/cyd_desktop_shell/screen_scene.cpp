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
#include "esp_heap_caps.h"
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

// The scene's memory, in small pieces held from the first present until the
// scene ends (app end, or the view leaving). Each group is either the app's
// bytearrays (cyd.py keeps them; nothing to free here) or taken from the
// ESP-IDF heap (owned, freed by release_pieces()).
struct Pieces {
    void *frame = nullptr;
    void *items[scene::kItemChunks]{};
    void *polys[scene::kPolyChunks]{};
    void *z_rows[scene::kBandRows]{};
    bool owns_2d = false;
    bool owns_3d = false;
};
Pieces pieces;

// 8-byte aligned: the engine keeps int64_t fields (plain heap_caps_malloc is 4).
void *take(std::size_t bytes) { return heap_caps_aligned_alloc(8, bytes, MALLOC_CAP_8BIT); }

template <std::size_t N> void release_all(void *(&list)[N], bool owned) {
    for (void *&piece : list) {
        if (owned) heap_caps_free(piece);
        piece = nullptr;
    }
}

// Called with the display lock held, after scene::end() (the view's leave()
// runs under it too).
void release_pieces() {
    if (pieces.owns_2d) heap_caps_free(pieces.frame);
    pieces.frame = nullptr;
    release_all(pieces.items, pieces.owns_2d);
    release_all(pieces.polys, pieces.owns_3d);
    release_all(pieces.z_rows, pieces.owns_3d);
    pieces.owns_2d = pieces.owns_3d = false;
}

// Uses the app's pieces when given (frame, then the lists), or takes owned ones.
template <std::size_t N> bool adopt_or_take(void *(&list)[N], void *const given[], std::size_t bytes, bool owned) {
    for (std::size_t index = 0; index < N; ++index) {
        if (!owned) list[index] = given[index];
        else if (list[index] == nullptr) list[index] = take(bytes);
        if (list[index] == nullptr) return false;
    }
    return true;
}


bool scene_view_in_front() {
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    return snapshot.view == cyd::desktop::View::Native &&
           native::has_flag(snapshot.foreground_app, native::kAppSceneSurface);
}

}  // namespace

// Sets up the scene's memory and brings its view to the front. frame and
// items (kItemChunks pieces) are the app's buffers, or null to take them from
// the ESP-IDF heap. 0 on success, -1 when the memory is not there, -5 without
// a scene surface, -6 when the display stayed busy.
extern "C" int cyd_desktop_scene_begin(void *frame, void *const items[]) {
    scene::scene_left = release_pieces;
    const auto *surface = native::find_with_flag(native::kAppSceneSurface);
    if (surface == nullptr) return -5;
    if (!take_display()) return -6;
    bool adopted = scene::ready();
    if (!adopted) {
        pieces.owns_2d = frame == nullptr;
        pieces.frame = pieces.owns_2d ? take(scene::frame_bytes()) : frame;
        adopted = pieces.frame != nullptr &&
                  adopt_or_take(pieces.items, items, scene::item_chunk_bytes(), pieces.owns_2d) &&
                  scene::begin(pieces.frame, scene::frame_bytes(), pieces.items, scene::item_chunk_bytes());
        if (!adopted) release_pieces();
    }
    xSemaphoreGive(display_mutex);
    if (!adopted) return -1;
    if (!scene_view_in_front()) {
        cyd::desktop::shell_state().launch_native(surface->id);
        cyd_desktop_ui_refresh();
    }
    return 0;
}

// Sets up the 3D polygon list and band Z buffer from the app's pieces, or
// from the ESP-IDF heap when polys is null. 0 on success, -1 when the memory
// is not there (the 2D scene is kept), -6 when the display stayed busy.
extern "C" int cyd_desktop_scene_begin_3d(void *const polys[], void *const z_rows[]) {
    if (!take_display()) return -6;
    bool adopted = scene::ready_3d();
    if (!adopted) {
        pieces.owns_3d = polys == nullptr;
        adopted = adopt_or_take(pieces.polys, polys, scene::poly_chunk_bytes(), pieces.owns_3d) &&
                  adopt_or_take(pieces.z_rows, z_rows, scene::z_row_bytes(), pieces.owns_3d) &&
                  scene::begin_3d(pieces.polys, scene::poly_chunk_bytes(), pieces.z_rows, scene::z_row_bytes());
        if (!adopted) {
            release_all(pieces.polys, pieces.owns_3d);
            release_all(pieces.z_rows, pieces.owns_3d);
            pieces.owns_3d = false;
        }
    }
    xSemaphoreGive(display_mutex);
    return adopted ? 0 : -1;
}

extern "C" void cyd_desktop_scene_end(void) {
    // The pointers must go before Python may collect the buffers, so drop them
    // even if the display stays busy.
    const bool locked = take_display();
    scene::end();
    release_pieces();
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
// overlay on top. The polygon count when shown; -1 before begin_3d() has
// given the 3D buffers, -2 for an invalid sprite record, -4 when the scene
// view is no longer in front, -6 when the display stayed busy.
extern "C" int cyd_desktop_scene_present_3d(const uint8_t *items, size_t item_count, const uint8_t *records,
                                            size_t records_size, int background, bool z_buffer) {
    if (!scene_view_in_front()) return -4;
    if (!scene::valid_records(records, records_size)) return scene::kBadRecords;
    if (!take_display()) return -6;
    const TickType_t started = xTaskGetTickCount();
    if (!scene::ready_3d()) {
        xSemaphoreGive(display_mutex);
        return -1;
    }
    // Building under the display lock keeps a UI-task redraw from seeing a
    // half-written polygon list.
    int polygons = 0;
    for (size_t index = 0; index < item_count && polygons < scene::kPolyCapacity; ++index) {
        const uint8_t *item = items + index * cyd::desktop::n3d::kItemBytes;
        int next_face = 0, previous;
        do {
            previous = next_face;
            const int used = polygons % scene::kPolyChunk;
            polygons += cyd::desktop::n3d::build_item(item, false, scene::poly_chunk(polygons / scene::kPolyChunk) + used,
                                                      scene::kPolyChunk - used, &next_face);
        } while (next_face != previous && polygons < scene::kPolyCapacity &&
                 polygons % scene::kPolyChunk == 0 && next_face > 0);
    }
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
