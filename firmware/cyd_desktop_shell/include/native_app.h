#pragma once

#include <cstddef>
#include <cstdint>

namespace cyd::desktop::native {

constexpr int kScreenWidth = 320;
constexpr int kContentHeight = 212;

constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3));
}

constexpr uint16_t byte_swap(uint16_t value) {
    return static_cast<uint16_t>((value << 8) | (value >> 8));
}

// A bounded view onto the shell's existing RGB565 transition tile. Native
// apps never own the LCD or a full-frame buffer; pixels outside this tile are
// intentionally inaccessible.
struct TileSurface {
    uint16_t *pixels;
    int width;
    int height;
    int stride;
    int origin_y;

    void pixel(int x, int y, uint16_t color) const {
        const int local_y = y - origin_y;
        if (x < 0 || x >= width || local_y < 0 || local_y >= height) return;
        pixels[static_cast<size_t>(local_y) * stride + x] = byte_swap(color);
    }

    void hline(int x, int y, int length, uint16_t color) const {
        const int local_y = y - origin_y;
        if (local_y < 0 || local_y >= height || length <= 0) return;
        const int left = x < 0 ? 0 : x;
        const int right = x + length > width ? width : x + length;
        if (right <= left) return;
        uint16_t *row = pixels + static_cast<size_t>(local_y) * stride;
        const uint16_t value = byte_swap(color);
        for (int column = left; column < right; ++column) row[column] = value;
    }

    void fill(uint16_t color) const {
        const uint16_t value = byte_swap(color);
        for (int y = 0; y < height; ++y) {
            uint16_t *row = pixels + static_cast<size_t>(y) * stride;
            for (int x = 0; x < width; ++x) row[x] = value;
        }
    }
};

enum class InputType : uint8_t { Down, Move, Up, Tap };

struct InputEvent {
    InputType type;
    int16_t x;
    int16_t y;
};

// What the shell may do with an app. The shell tests these flags; it never
// compares app ids, so a new app gets the right treatment by declaring them.
enum AppFlags : uint8_t {
    kAppLaunchable = 1 << 0,     // may be started by id (Home launcher, desktop_launch)
    kAppScriptHosted = 1 << 1,   // a foreground MicroPython app drives this view
    kAppVideoSurface = 1 << 2,   // the target of cyd.video_begin() / cyd.video_present()
};

// Glyph used for the Home launcher. Maps onto the shell's built-in icons.
enum class LauncherIcon : uint8_t { None, Game, Movie };

// Receives the semantic controls an app wants listed in desktop_ui_tree, in
// absolute display coordinates.
using UiNodeFn = void (*)(const char *id, const char *role, int x, int y,
                          int width, int height, const char *label);

struct NativeApp {
    const char *id;
    const char *title;
    uint16_t target_frame_ms;
    void (*enter)();
    void (*leave)();
    bool (*update)(uint32_t delta_ms);
    void (*input)(const InputEvent &event);
    void (*render_tile)(const TileSurface &surface);

    uint8_t flags;
    // Home launcher. A null label keeps the app off Home. Home has a single
    // native slot, filled by the first registered app that sets a label.
    const char *launcher_label;
    uint16_t launcher_color;
    LauncherIcon launcher_icon;
    // Optional. Called while the app is in front so its on-screen controls show
    // up in desktop_ui_tree; ids should start with "native.".
    void (*describe_ui)(UiNodeFn emit);
};

struct RuntimeMetrics {
    uint32_t frames;
    uint32_t last_frame_ms;
    uint32_t average_frame_ms;
};

// Registry (native_registry.cpp is the only place apps are listed).
std::size_t app_count();
const NativeApp *app_at(std::size_t index);
const NativeApp *find(const char *id);
const NativeApp *find_with_flag(uint8_t flag);
const NativeApp *home_launcher();
bool has_flag(const char *id, uint8_t flag);

bool launch(const char *id);
void leave();
bool active();
const NativeApp *current();
bool update(uint32_t delta_ms);
void input(const InputEvent &event);
void render_tile(const TileSurface &surface);
void note_frame(uint32_t elapsed_ms);
RuntimeMetrics metrics();

}  // namespace cyd::desktop::native
