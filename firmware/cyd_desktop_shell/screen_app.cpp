// The foreground MicroPython app's retained UI: the windowed canvas with
// texts and buttons, and the full-screen game and console modes.

#include "screen/views.h"
#include "screen/keyboard.h"
#include "screen/widgets.h"
#include "native_app.h"
#include "shell_state.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace cyd::desktop::screen {

namespace {

struct AppText { int x; int y; char value[49]; };
struct AppButton { int x; int y; int width; int height; char id[17]; char label[25]; };
enum class AppPrimitiveType : uint8_t {
    Pixel = 0,
    Line = 1,
    Rect = 2,
    FillRect = 3,
    Circle = 4,
    FillCircle = 5,
};
struct AppPrimitive {
    AppPrimitiveType type;
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    uint16_t color;
    uint8_t stroke;
};
// Per-frame capacity of a MicroPython app's retained UI. Documented in
// docs/cyd-api.md; bridge/sdk-docs.test.mjs keeps the two in step.
constexpr uint8_t kAppTextCapacity = 10;
constexpr uint8_t kAppButtonCapacity = 4;
constexpr uint8_t kAppPrimitiveCapacity = 48;
struct GameSprite {
    int16_t x;
    int16_t y;
    uint16_t color;
    uint8_t kind;
    uint8_t size;
};
constexpr uint8_t kGameSpriteCapacity = 128;
struct GameSurface {
    bool active;
    uint16_t background_top;
    uint16_t background_bottom;
    uint16_t accent;
    uint32_t frame;
    uint32_t score;
    uint8_t lives;
    uint8_t bombs;
    uint8_t sprite_count;
    GameSprite sprites[kGameSpriteCapacity];
};
struct AppSurface {
    char app_id[25];
    char title[25];
    char log[49];
    AppText texts[kAppTextCapacity];
    AppButton buttons[kAppButtonCapacity];
    AppPrimitive primitives[kAppPrimitiveCapacity];
    uint8_t text_count;
    uint8_t button_count;
    uint8_t primitive_count;
    bool game_mode;
    bool console_mode;
};
AppSurface app_surface{};
portMUX_TYPE app_surface_lock = portMUX_INITIALIZER_UNLOCKED;
GameSurface game_surface{};
GameSurface rendering_game_surface{};
portMUX_TYPE game_surface_lock = portMUX_INITIALIZER_UNLOCKED;
bool displayed_game = false;

// Lines the console can hold. How many are visible depends on the options:
// 10 above the on-screen keyboard, 21 above the input line without it, and
// all 22 with neither.
constexpr int kConsoleMaxLines = 22;
constexpr int kConsoleLinesWithKeyboard = 10;
constexpr int kConsoleLineTop = 15;
constexpr int kConsoleLinePitch = 9;
struct ConsoleSurface {
    bool active;
    char lines[kConsoleMaxLines][52];
    uint16_t line_colors[kConsoleMaxLines];
    uint8_t line_count;
    char prompt[8];
    char input[52];
    uint8_t cursor_pos;
    uint8_t kb_mode;      // 0: lower, 1: upper, 2: sym
    int8_t pressed_key;   // -1: none
    bool show_keyboard;
    bool show_input;
};
ConsoleSurface console_surface{};
ConsoleSurface rendering_console_surface{};
portMUX_TYPE console_surface_lock = portMUX_INITIALIZER_UNLOCKED;
bool displayed_console = false;

// App content may use the window body and the strip below the window.  Keep
// the one-pixel window frame and the taskbar out of the app's drawing clip.
constexpr Rect kAppBodyBounds{14, 44, 292, 158};
constexpr Rect kAppFooterBounds{0, 0, 0, 0};

struct AppDisplayState {
    bool valid;
    bool custom_app;
    char shell_title[25];
    AppSurface surface;
};
AppDisplayState displayed_app{};

Rect text_bounds(const char *value, int x, int y, int scale) {
    if (value == nullptr || value[0] == '\0') return {0, 0, 0, 0};
    return {x, y, static_cast<int>(std::strlen(value)) * 6 * scale, 7 * scale};
}

Rect button_bounds(const AppButton &button) {
    return {button.x, button.y, button.width, button.height};
}

Rect primitive_bounds(const AppPrimitive &primitive) {
    const int stroke = std::max(1, static_cast<int>(primitive.stroke));
    const int padding = (stroke + 1) / 2;
    switch (primitive.type) {
        case AppPrimitiveType::Pixel:
            return {primitive.x0, primitive.y0, 1, 1};
        case AppPrimitiveType::Line: {
            const int left = std::min(static_cast<int>(primitive.x0), static_cast<int>(primitive.x1)) - padding;
            const int top = std::min(static_cast<int>(primitive.y0), static_cast<int>(primitive.y1)) - padding;
            const int right = std::max(static_cast<int>(primitive.x0), static_cast<int>(primitive.x1)) + padding;
            const int bottom = std::max(static_cast<int>(primitive.y0), static_cast<int>(primitive.y1)) + padding;
            return {left, top, right - left + 1, bottom - top + 1};
        }
        case AppPrimitiveType::Rect:
            return {primitive.x0 - padding, primitive.y0 - padding,
                    primitive.x1 + padding * 2, primitive.y1 + padding * 2};
        case AppPrimitiveType::FillRect:
            return {primitive.x0, primitive.y0, primitive.x1, primitive.y1};
        case AppPrimitiveType::Circle:
            return {primitive.x0 - primitive.x1 - padding, primitive.y0 - primitive.x1 - padding,
                    primitive.x1 * 2 + padding * 2 + 1, primitive.x1 * 2 + padding * 2 + 1};
        case AppPrimitiveType::FillCircle:
            return {primitive.x0 - primitive.x1, primitive.y0 - primitive.x1,
                    primitive.x1 * 2 + 1, primitive.x1 * 2 + 1};
    }
    return {0, 0, 0, 0};
}

void add_app_dirty(DirtyRects *dirty, const Rect &rect) {
    dirty->add(rect, kAppBodyBounds);
    dirty->add(rect, kAppFooterBounds);
}

void draw_app_button(const AppButton &button, bool pressed) {
    const uint16_t fill = pressed ? kAccent : kWindowBlue;
    fill_round_rect(button.x + 1, button.y + 2, button.width, button.height, 7, kShadow);
    round_rect(button.x, button.y, button.width, button.height, 7,
               pressed ? kWhite : kAccent, fill);
    centered_text_fit(button.label, button.x + button.width / 2,
                      button.y + (button.height - 7) / 2 + (pressed ? 1 : 0),
                      button.width - 10, kWhite);
}

void draw_app_primitive(const AppPrimitive &primitive) {
    const int stroke = std::max(1, static_cast<int>(primitive.stroke));
    switch (primitive.type) {
        case AppPrimitiveType::Pixel:
            fill_rect(primitive.x0, primitive.y0, 1, 1, primitive.color);
            break;
        case AppPrimitiveType::Line:
            line(primitive.x0, primitive.y0, primitive.x1, primitive.y1, primitive.color, stroke);
            break;
        case AppPrimitiveType::Rect:
            for (int inset = 0; inset < stroke; ++inset)
                frame(primitive.x0 + inset, primitive.y0 + inset,
                      primitive.x1 - inset * 2, primitive.y1 - inset * 2, primitive.color);
            break;
        case AppPrimitiveType::FillRect:
            fill_rect(primitive.x0, primitive.y0, primitive.x1, primitive.y1, primitive.color);
            break;
        case AppPrimitiveType::Circle:
            circle(primitive.x0, primitive.y0, primitive.x1, primitive.color, stroke);
            break;
        case AppPrimitiveType::FillCircle:
            fill_circle(primitive.x0, primitive.y0, primitive.x1, primitive.color);
            break;
    }
}

void draw_app_dynamic(const AppSurface &surface, bool custom_app) {
    if (!custom_app) {
        fill_round_rect(23, 62, 274, 58, 10, kCardAlt);
        draw_icon(UiIcon::Scripts, 38, 77, kWindowBlue);
        text("APP SHELL READY", 66, 73, kBlack, 2);
        text("Use the taskbar to return Home", 66, 99, kDarkGray);
        return;
    }
    // Canvas is the back layer. Text, buttons and status are replayed after it,
    // so dirty redraws preserve the same retained-mode z-order.
    for (uint8_t index = 0; index < surface.primitive_count; ++index) {
        const auto &primitive = surface.primitives[index];
        if (clip_enabled && rect_empty(intersect_rect(primitive_bounds(primitive), drawing_clip))) continue;
        draw_app_primitive(primitive);
    }
    if (surface.title[0] != '\0') text(surface.title, 22, 54, kBlack, 2);
    for (uint8_t index = 0; index < surface.text_count; ++index)
        text(surface.texts[index].value, surface.texts[index].x, surface.texts[index].y, kBlack, 1);
    for (uint8_t index = 0; index < surface.button_count; ++index) {
        const auto &button = surface.buttons[index];
        draw_app_button(button, false);
    }
    if (surface.log[0] != '\0') text(surface.log, 18, 194, kDarkGray, 1);
}

void draw_app_dynamic_clipped(const AppSurface &surface, bool custom_app, const Rect &clip) {
    set_clip(clip);
    draw_app_dynamic(surface, custom_app);
    clear_clip();
}

void draw_all_app_dynamic(const AppSurface &surface, bool custom_app) {
    draw_app_dynamic_clipped(surface, custom_app, kAppBodyBounds);
    draw_app_dynamic_clipped(surface, custom_app, kAppFooterBounds);
}

void register_app_surface(const AppSurface &surface, bool custom_app) {
    ui_node("app.window", "window", 8, 9, 304, 196,
            custom_app ? surface.title : "Application");
    if (!custom_app) return;
    if (surface.primitive_count > 0) {
        char label[25];
        std::snprintf(label, sizeof(label), "%u primitives", surface.primitive_count);
        ui_node("app.canvas", "canvas", kAppBodyBounds.x, kAppBodyBounds.y,
                kAppBodyBounds.width, kAppBodyBounds.height, label, false);
    }
    for (uint8_t index = 0; index < surface.text_count; ++index) {
        char node_id[41];
        std::snprintf(node_id, sizeof(node_id), "app.text.%u", index);
        ui_node(node_id, "text", surface.texts[index].x, surface.texts[index].y,
                text_width(surface.texts[index].value), 7, surface.texts[index].value, false);
    }
    for (uint8_t index = 0; index < surface.button_count; ++index) {
        const auto &button = surface.buttons[index];
        char node_id[41];
        std::snprintf(node_id, sizeof(node_id), "app.button.%s", button.id);
        ui_node(node_id, "button", button.x, button.y, button.width, button.height, button.label);
    }
    if (surface.log[0] != '\0')
        ui_node("app.log", "status", 18, 194, text_width(surface.log), 7, surface.log, false);
}

void remember_app(const char *title, const AppSurface &surface, bool custom_app) {
    displayed_app.valid = true;
    displayed_app.custom_app = custom_app;
    std::snprintf(displayed_app.shell_title, sizeof(displayed_app.shell_title), "%s", title);
    displayed_app.surface = surface;
}

void paint_app_full(const char *title, const AppSurface &surface, bool custom_app) {
    wallpaper();
    card(8, 9, 304, 196, 13, kCard);
    fill_round_rect(10, 11, 300, 33, 11, kWindowBlue);
    fill_rect(10, 31, 300, 13, kWindowBlue);
    draw_icon(UiIcon::Scripts, 20, 19, kWhite);
    text(title, 45, 23, kWhite);
    draw_all_app_dynamic(surface, custom_app);
    taskbar();
}

void render_app_full(const char *title, const AppSurface &surface, bool custom_app) {
    render_in_transition_tiles([title, &surface, custom_app]() {
        paint_app_full(title, surface, custom_app);
    });
    remember_app(title, surface, custom_app);
}

void draw_game_sprite(const GameSprite &sprite) {
    const int size = std::max(1, static_cast<int>(sprite.size));
    switch (sprite.kind) {
        case 0:  // player ship
            line(sprite.x, sprite.y - size, sprite.x - size, sprite.y + size, sprite.color, 2);
            line(sprite.x, sprite.y - size, sprite.x + size, sprite.y + size, sprite.color, 2);
            fill_rect(sprite.x - 2, sprite.y - size / 2, 5, size + 2, kWhite);
            fill_rect(sprite.x - size + 2, sprite.y + size - 2, size * 2 - 3, 2, kCyan);
            break;
        case 1:  // enemy
            fill_circle(sprite.x, sprite.y, size, sprite.color);
            circle(sprite.x, sprite.y, std::max(2, size - 2), kWhite, 1);
            fill_rect(sprite.x - size - 3, sprite.y - 2, size * 2 + 7, 4, sprite.color);
            fill_circle(sprite.x, sprite.y, 2, kBlack);
            break;
        case 2:  // enemy bullet
            fill_circle(sprite.x, sprite.y, size, sprite.color);
            if (size >= 3) fill_circle(sprite.x - 1, sprite.y - 1, 1, kWhite);
            break;
        case 3:  // player shot
            fill_rect(sprite.x - std::max(1, size / 3), sprite.y - size,
                      std::max(2, size * 2 / 3), size * 2, sprite.color);
            fill_rect(sprite.x, sprite.y - size, 1, size * 2, kWhite);
            break;
        case 4:  // explosion / collectible
            circle(sprite.x, sprite.y, size, sprite.color, 2);
            line(sprite.x - size, sprite.y, sprite.x + size, sprite.y, kWhite, 1);
            line(sprite.x, sprite.y - size, sprite.x, sprite.y + size, kWhite, 1);
            break;
        default:
            fill_rect(sprite.x - size / 2, sprite.y - size / 2, size, size, sprite.color);
            break;
    }
}

void draw_game_surface(const char *title, const GameSurface &game) {
    fill_rect(0, 0, kWidth, 18, 0x0863);
    fill_rect(0, 18, kWidth, 48, game.background_top);
    fill_rect(0, 66, kWidth, 48, static_cast<uint16_t>((game.background_top & 0xf7de) + (game.background_bottom & 0x0821)));
    fill_rect(0, 114, kWidth, 49, game.background_bottom);
    fill_rect(0, 163, kWidth, 49, static_cast<uint16_t>((game.background_bottom & 0xf7de) >> 1));
    for (int index = 0; index < 28; ++index) {
        const int speed = index % 3 + 1;
        const int x = 4 + (index * 73 + index * index * 11) % 312;
        const int y = 20 + (index * 37 + static_cast<int>(game.frame) * speed) % 188;
        if (tile_raster_active && (y + 1 < tile_raster_y || y >= tile_raster_y + tile_raster_height))
            continue;
        fill_rect(x, y, speed == 3 ? 2 : 1, speed == 1 ? 1 : 2,
                  index % 5 == 0 ? game.accent : kWhite);
    }
    fill_rect(0, 18, 2, 194, game.accent);
    fill_rect(318, 18, 2, 194, game.accent);
    for (uint8_t index = 0; index < game.sprite_count; ++index) {
        const auto &sprite = game.sprites[index];
        const int radius = static_cast<int>(sprite.size) + 4;
        if (tile_raster_active &&
            (sprite.y + radius < tile_raster_y || sprite.y - radius >= tile_raster_y + tile_raster_height))
            continue;
        draw_game_sprite(sprite);
    }
    char score[32];
    std::snprintf(score, sizeof(score), "SCORE %07lu", static_cast<unsigned long>(game.score));
    text(title, 6, 6, kCyan);
    text(score, 126, 6, kWhite);
    for (uint8_t life = 0; life < game.lives && life < 5; ++life)
        fill_circle(278 + life * 8, 9, 3, kGreen);
    for (uint8_t bomb = 0; bomb < game.bombs && bomb < 3; ++bomb)
        frame(278 + bomb * 10, 20, 6, 6, kYellow);
}

void render_game(const char *title, const AppSurface &surface) {
    taskENTER_CRITICAL(&game_surface_lock);
    rendering_game_surface = game_surface;
    taskEXIT_CRITICAL(&game_surface_lock);
    const bool first_frame = !displayed_game || !displayed_app.valid ||
        std::strcmp(displayed_app.shell_title, title) != 0;
    for (int y = 0; y < kTaskbarY; y += kTransitionTileRows) {
        const int height = std::min(kTransitionTileRows, kTaskbarY - y);
        begin_transition_tile(y, height);
        draw_game_surface(title, rendering_game_surface);
        flush_transition_tile();
    }
    if (first_frame) {
        for (int y = kTaskbarY; y < kHeight; y += kTransitionTileRows) {
            begin_transition_tile(y, std::min(kTransitionTileRows, kHeight - y));
            taskbar();
            flush_transition_tile();
        }
    }
    char label[25];
    std::snprintf(label, sizeof(label), "%u sprites", rendering_game_surface.sprite_count);
    ui_node("app.game", "canvas", 0, 0, kWidth, kTaskbarY, label, false);
    displayed_game = true;
    remember_app(title, surface, true);
}

void draw_console_surface(const char *title, const ConsoleSurface &console) {
    fill_rect(0, 0, kWidth, 14, 0x0863);
    text(title, 4, 3, kCyan);
    text("MicroPython REPL", 180, 3, 0x7BEF);

    const int visible = console.show_keyboard ? kConsoleLinesWithKeyboard
                        : (console.show_input ? kConsoleMaxLines - 1 : kConsoleMaxLines);
    const int input_y = kConsoleLineTop + visible * kConsoleLinePitch;
    const int bottom = console.show_keyboard ? 116 : kTaskbarY;
    fill_rect(0, 14, kWidth, bottom - 14, kBlack);
    for (int i = 0; i < console.line_count && i < visible; ++i) {
        if (console.lines[i][0] != '\0') {
            text(console.lines[i], 4, kConsoleLineTop + i * kConsoleLinePitch,
                 console.line_colors[i] == 0 ? kWhite : console.line_colors[i]);
        }
    }

    if (console.show_input) {
        // The keyboard layout leaves the input line at y 105, one pitch short of 10 lines.
        const int y = console.show_keyboard ? 105 : input_y;
        text(console.prompt, 4, y, kGreen);
        const int input_x = 4 + text_width(console.prompt);
        text(console.input, input_x, y, kWhite);
        const int cursor_x = input_x + console.cursor_pos * 6;
        if (cursor_x < kWidth - 4) {
            fill_rect(cursor_x, y + 7, 5, 2, kGreen);
        }
    }

    if (console.show_keyboard) keyboard::draw(console.kb_mode, console.pressed_key);
}

void render_console(const char *title, const AppSurface &surface) {
    taskENTER_CRITICAL(&console_surface_lock);
    rendering_console_surface = console_surface;
    taskEXIT_CRITICAL(&console_surface_lock);
    const bool first_frame = !displayed_console || !displayed_app.valid ||
        std::strcmp(displayed_app.shell_title, title) != 0;
    for (int y = 0; y < kTaskbarY; y += kTransitionTileRows) {
        const int height = std::min(kTransitionTileRows, kTaskbarY - y);
        begin_transition_tile(y, height);
        draw_console_surface(title, rendering_console_surface);
        flush_transition_tile();
    }
    if (first_frame) {
        for (int y = kTaskbarY; y < kHeight; y += kTransitionTileRows) {
            begin_transition_tile(y, std::min(kTransitionTileRows, kHeight - y));
            taskbar();
            flush_transition_tile();
        }
    }
    ui_node("app.console", "terminal", 0, 0, kWidth, kTaskbarY,
            rendering_console_surface.show_keyboard ? "Console" : "Console (no keyboard)", false);
    displayed_console = true;
    remember_app(title, surface, true);
}

bool app_text_equal(const AppText &a, const AppText &b) {
    return a.x == b.x && a.y == b.y && std::strcmp(a.value, b.value) == 0;
}

bool app_button_equal(const AppButton &a, const AppButton &b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height &&
           std::strcmp(a.id, b.id) == 0 && std::strcmp(a.label, b.label) == 0;
}

bool app_primitive_equal(const AppPrimitive &a, const AppPrimitive &b) {
    return a.type == b.type && a.x0 == b.x0 && a.y0 == b.y0 &&
           a.x1 == b.x1 && a.y1 == b.y1 && a.color == b.color && a.stroke == b.stroke;
}

void restore_app_background(const Rect &dirty) {
    fill_rect(dirty.x, dirty.y, dirty.width, dirty.height, kCard);
}

}  // namespace

void render_app(const char *title) {
    AppSurface surface{};
    taskENTER_CRITICAL(&app_surface_lock);
    surface = app_surface;
    taskEXIT_CRITICAL(&app_surface_lock);
    const bool custom_app = surface.app_id[0] != '\0' && std::strcmp(surface.app_id, title) == 0;
    if (custom_app && surface.game_mode) {
        render_game(surface.title[0] == '\0' ? title : surface.title, surface);
        return;
    }
    if (custom_app && surface.console_mode) {
        render_console(surface.title[0] == '\0' ? title : surface.title, surface);
        return;
    }
    displayed_game = false;
    displayed_console = false;
    if (!displayed_app.valid || displayed_app.custom_app != custom_app ||
        std::strcmp(displayed_app.shell_title, title) != 0) {
        render_app_full(title, surface, custom_app);
        register_app_surface(surface, custom_app);
        return;
    }

    if (!custom_app) {
        remember_app(title, surface, false);
        register_app_surface(surface, false);
        return;
    }

    DirtyRects dirty;
    const AppSurface &old = displayed_app.surface;
    if (std::strcmp(old.title, surface.title) != 0) {
        add_app_dirty(&dirty, text_bounds(old.title, 22, 54, 2));
        add_app_dirty(&dirty, text_bounds(surface.title, 22, 54, 2));
    }
    const uint8_t text_count = std::max(old.text_count, surface.text_count);
    for (uint8_t index = 0; index < text_count; ++index) {
        const bool had_old = index < old.text_count;
        const bool has_new = index < surface.text_count;
        if (had_old && has_new && app_text_equal(old.texts[index], surface.texts[index])) continue;
        if (had_old) add_app_dirty(&dirty, text_bounds(old.texts[index].value, old.texts[index].x, old.texts[index].y, 1));
        if (has_new) add_app_dirty(&dirty, text_bounds(surface.texts[index].value, surface.texts[index].x, surface.texts[index].y, 1));
    }
    const uint8_t button_count = std::max(old.button_count, surface.button_count);
    for (uint8_t index = 0; index < button_count; ++index) {
        const bool had_old = index < old.button_count;
        const bool has_new = index < surface.button_count;
        if (had_old && has_new && app_button_equal(old.buttons[index], surface.buttons[index])) continue;
        if (had_old) add_app_dirty(&dirty, button_bounds(old.buttons[index]));
        if (has_new) add_app_dirty(&dirty, button_bounds(surface.buttons[index]));
    }
    const uint8_t primitive_count = std::max(old.primitive_count, surface.primitive_count);
    for (uint8_t index = 0; index < primitive_count; ++index) {
        const bool had_old = index < old.primitive_count;
        const bool has_new = index < surface.primitive_count;
        if (had_old && has_new && app_primitive_equal(old.primitives[index], surface.primitives[index])) continue;
        if (had_old) add_app_dirty(&dirty, primitive_bounds(old.primitives[index]));
        if (has_new) add_app_dirty(&dirty, primitive_bounds(surface.primitives[index]));
    }
    if (std::strcmp(old.log, surface.log) != 0) {
        add_app_dirty(&dirty, text_bounds(old.log, 18, 194, 1));
        add_app_dirty(&dirty, text_bounds(surface.log, 18, 194, 1));
    }

    // Compose each dirty band completely in RAM. The LCD receives old pixels
    // -> finished new pixels without exposing the intermediate background.
    dirty.sort_top_to_bottom();
    for (uint8_t index = 0; index < dirty.count; ++index) {
        const Rect &region = dirty.values[index];
        for (int y = region.y; y < region.y + region.height; y += kTransitionTileRows) {
            const Rect band{region.x, y, region.width,
                            std::min(kTransitionTileRows, region.y + region.height - y)};
            begin_transition_tile(band.y, band.height);
            restore_app_background(band);
            draw_app_dynamic_clipped(surface, true, band);
            flush_transition_rect(band);
        }
    }
    remember_app(title, surface, true);
    register_app_surface(surface, true);
}

namespace {

bool app_button_hit(int x, int y) {
    char event[32] = {};
    taskENTER_CRITICAL(&app_surface_lock);
    for (uint8_t index = 0; index < app_surface.button_count; ++index) {
        const auto &button = app_surface.buttons[index];
        if (x >= button.x && x < button.x + button.width && y >= button.y && y < button.y + button.height) {
            std::snprintf(event, sizeof(event), "tap.%s", button.id);
            break;
        }
    }
    taskEXIT_CRITICAL(&app_surface_lock);
    if (event[0] == '\0') return false;
    record_input(event);
    return true;
}

bool redraw_app_button_at(int x, int y, bool pressed) {
    AppButton selected{};
    bool found = false;
    taskENTER_CRITICAL(&app_surface_lock);
    for (uint8_t index = 0; index < app_surface.button_count; ++index) {
        const auto &button = app_surface.buttons[index];
        if (x >= button.x && x < button.x + button.width &&
            y >= button.y && y < button.y + button.height) {
            selected = button;
            found = true;
            break;
        }
    }
    taskEXIT_CRITICAL(&app_surface_lock);
    if (!found) return false;
    set_clip(kAppBodyBounds);
    draw_app_button(selected, pressed);
    clear_clip();
    return true;
}

}  // namespace

bool app_game_mode() { return app_surface.game_mode; }

void app_touch_down(int x, int y) { redraw_app_button_at(x, y, true); }

void handle_app_touch(int x, int y) {
    redraw_app_button_at(x, y, false);
    if (app_button_hit(x, y)) render();
}

void app_view_invalidate() { displayed_app.valid = false; }

void app_view_forget_game() { displayed_game = false; }

}  // namespace cyd::desktop::screen

using namespace cyd::desktop::screen;

extern "C" void cyd_desktop_app_keys_clear(void);

extern "C" void cyd_desktop_app_begin(const char *app_id) {
    cyd_desktop_app_keys_clear();
    taskENTER_CRITICAL(&app_surface_lock);
    app_surface = {};
    std::snprintf(app_surface.app_id, sizeof(app_surface.app_id), "%s", app_id == nullptr ? "" : app_id);
    taskEXIT_CRITICAL(&app_surface_lock);
    taskENTER_CRITICAL(&game_surface_lock);
    game_surface = {};
    taskEXIT_CRITICAL(&game_surface_lock);
    cyd::desktop::shell_state().launch_script(app_id);
    // Keep the launcher visible until the app presents its first complete
    // retained frame. app_update() then performs one tiled window transition
    // instead of exposing an empty window followed by foreground content.
}

extern "C" void cyd_desktop_app_title(const char *value) {
    taskENTER_CRITICAL(&app_surface_lock);
    std::snprintf(app_surface.title, sizeof(app_surface.title), "%s", value == nullptr ? "" : value);
    taskEXIT_CRITICAL(&app_surface_lock);
}

extern "C" void cyd_desktop_app_clear(void) {
    taskENTER_CRITICAL(&app_surface_lock);
    app_surface.text_count = 0;
    app_surface.button_count = 0;
    app_surface.primitive_count = 0;
    app_surface.game_mode = false;
    app_surface.console_mode = false;
    app_surface.log[0] = '\0';
    taskEXIT_CRITICAL(&app_surface_lock);
    taskENTER_CRITICAL(&game_surface_lock);
    game_surface.active = false;
    game_surface.sprite_count = 0;
    taskEXIT_CRITICAL(&game_surface_lock);
}

extern "C" void cyd_desktop_game_begin(int background_top, int background_bottom, int accent) {
    taskENTER_CRITICAL(&app_surface_lock);
    app_surface.text_count = 0;
    app_surface.button_count = 0;
    app_surface.primitive_count = 0;
    app_surface.game_mode = true;
    taskEXIT_CRITICAL(&app_surface_lock);
    taskENTER_CRITICAL(&game_surface_lock);
    game_surface = {};
    game_surface.active = true;
    game_surface.background_top = static_cast<uint16_t>(background_top);
    game_surface.background_bottom = static_cast<uint16_t>(background_bottom);
    game_surface.accent = static_cast<uint16_t>(accent);
    game_surface.lives = 3;
    game_surface.bombs = 2;
    taskEXIT_CRITICAL(&game_surface_lock);
}

extern "C" void cyd_desktop_game_frame(uint32_t frame_number, uint32_t score, int lives, int bombs) {
    taskENTER_CRITICAL(&game_surface_lock);
    game_surface.frame = frame_number;
    game_surface.score = score;
    game_surface.lives = static_cast<uint8_t>(std::max(0, std::min(5, lives)));
    game_surface.bombs = static_cast<uint8_t>(std::max(0, std::min(3, bombs)));
    game_surface.sprite_count = 0;
    taskEXIT_CRITICAL(&game_surface_lock);
}

extern "C" bool cyd_desktop_game_sprite(int kind, int x, int y, int color, int size) {
    if (kind < 0 || kind > 4) return false;
    taskENTER_CRITICAL(&game_surface_lock);
    if (!game_surface.active || game_surface.sprite_count >= kGameSpriteCapacity) {
        taskEXIT_CRITICAL(&game_surface_lock);
        return false;
    }
    auto &sprite = game_surface.sprites[game_surface.sprite_count++];
    sprite.kind = static_cast<uint8_t>(kind);
    sprite.x = static_cast<int16_t>(std::max(-16, std::min(kWidth + 16, x)));
    sprite.y = static_cast<int16_t>(std::max(-16, std::min(kTaskbarY + 16, y)));
    sprite.color = static_cast<uint16_t>(color);
    sprite.size = static_cast<uint8_t>(std::max(1, std::min(16, size)));
    taskEXIT_CRITICAL(&game_surface_lock);
    return true;
}

extern "C" bool cyd_desktop_app_primitive(int type, int x0, int y0, int x1, int y1,
                                            int color, int stroke) {
    if (type < static_cast<int>(AppPrimitiveType::Pixel) ||
        type > static_cast<int>(AppPrimitiveType::FillCircle)) return false;
    taskENTER_CRITICAL(&app_surface_lock);
    if (app_surface.primitive_count >= kAppPrimitiveCapacity) {
        taskEXIT_CRITICAL(&app_surface_lock);
        return false;
    }
    auto &primitive = app_surface.primitives[app_surface.primitive_count++];
    primitive.type = static_cast<AppPrimitiveType>(type);
    primitive.x0 = static_cast<int16_t>(std::max(-320, std::min(640, x0)));
    primitive.y0 = static_cast<int16_t>(std::max(-240, std::min(480, y0)));
    if (primitive.type == AppPrimitiveType::Line) {
        primitive.x1 = static_cast<int16_t>(std::max(-320, std::min(640, x1)));
        primitive.y1 = static_cast<int16_t>(std::max(-240, std::min(480, y1)));
    } else if (primitive.type == AppPrimitiveType::Rect ||
               primitive.type == AppPrimitiveType::FillRect) {
        primitive.x1 = static_cast<int16_t>(std::max(1, std::min(kWidth, x1)));
        primitive.y1 = static_cast<int16_t>(std::max(1, std::min(kHeight, y1)));
    } else if (primitive.type == AppPrimitiveType::Circle ||
               primitive.type == AppPrimitiveType::FillCircle) {
        primitive.x1 = static_cast<int16_t>(std::max(1, std::min(kWidth, x1)));
        primitive.y1 = 0;
    } else {
        primitive.x1 = 0;
        primitive.y1 = 0;
    }
    primitive.color = static_cast<uint16_t>(color);
    primitive.stroke = static_cast<uint8_t>(std::max(1, std::min(8, stroke)));
    taskEXIT_CRITICAL(&app_surface_lock);
    return true;
}

extern "C" bool cyd_desktop_app_text(int x, int y, const char *value) {
    taskENTER_CRITICAL(&app_surface_lock);
    if (app_surface.text_count >= kAppTextCapacity) { taskEXIT_CRITICAL(&app_surface_lock); return false; }
    auto &entry = app_surface.texts[app_surface.text_count++];
    entry.x = x < 16 ? 16 : (x > 300 ? 300 : x);
    entry.y = y < 47 ? 47 : (y > 199 ? 199 : y);
    std::snprintf(entry.value, sizeof(entry.value), "%s", value == nullptr ? "" : value);
    taskEXIT_CRITICAL(&app_surface_lock);
    return true;
}

extern "C" bool cyd_desktop_app_button(const char *id, int x, int y, int width, int height, const char *label) {
    taskENTER_CRITICAL(&app_surface_lock);
    if (app_surface.button_count >= kAppButtonCapacity || id == nullptr || id[0] == '\0') { taskEXIT_CRITICAL(&app_surface_lock); return false; }
    auto &button = app_surface.buttons[app_surface.button_count++];
    button.x = x < 16 ? 16 : (x > 290 ? 290 : x);
    button.y = y < 47 ? 47 : (y > 190 ? 190 : y);
    button.width = width < 24 ? 24 : (width > 140 ? 140 : width);
    button.height = height < 16 ? 16 : (height > 48 ? 48 : height);
    std::snprintf(button.id, sizeof(button.id), "%s", id);
    std::snprintf(button.label, sizeof(button.label), "%s", label == nullptr ? "" : label);
    taskEXIT_CRITICAL(&app_surface_lock);
    return true;
}

extern "C" void cyd_desktop_app_log(const char *value) {
    taskENTER_CRITICAL(&app_surface_lock);
    const bool changed = std::strcmp(app_surface.log, value == nullptr ? "" : value) != 0;
    std::snprintf(app_surface.log, sizeof(app_surface.log), "%s", value == nullptr ? "" : value);
    taskEXIT_CRITICAL(&app_surface_lock);
    if (changed) record_event(value);
}

extern "C" void cyd_desktop_console_begin(void) {
    taskENTER_CRITICAL(&app_surface_lock);
    app_surface.text_count = 0;
    app_surface.button_count = 0;
    app_surface.primitive_count = 0;
    app_surface.game_mode = false;
    app_surface.console_mode = true;
    taskEXIT_CRITICAL(&app_surface_lock);
    taskENTER_CRITICAL(&console_surface_lock);
    console_surface = {};
    console_surface.active = true;
    console_surface.pressed_key = -1;
    console_surface.show_keyboard = true;
    console_surface.show_input = true;
    std::snprintf(console_surface.prompt, sizeof(console_surface.prompt), ">>> ");
    taskEXIT_CRITICAL(&console_surface_lock);
}

extern "C" void cyd_desktop_console_line(int index, const char *line_text, int color) {
    if (index < 0 || index >= kConsoleMaxLines) return;
    taskENTER_CRITICAL(&console_surface_lock);
    std::snprintf(console_surface.lines[index], sizeof(console_surface.lines[index]), "%s", line_text ? line_text : "");
    console_surface.line_colors[index] = static_cast<uint16_t>(color);
    if (index >= console_surface.line_count) console_surface.line_count = index + 1;
    taskEXIT_CRITICAL(&console_surface_lock);
}

extern "C" void cyd_desktop_console_input(const char *input_text, int cursor_pos, const char *prompt) {
    taskENTER_CRITICAL(&console_surface_lock);
    if (prompt != nullptr) std::snprintf(console_surface.prompt, sizeof(console_surface.prompt), "%s", prompt);
    std::snprintf(console_surface.input, sizeof(console_surface.input), "%s", input_text ? input_text : "");
    console_surface.cursor_pos = static_cast<uint8_t>(std::max(0, std::min(50, cursor_pos)));
    taskEXIT_CRITICAL(&console_surface_lock);
}

extern "C" void cyd_desktop_console_options(bool show_keyboard, bool show_input) {
    taskENTER_CRITICAL(&console_surface_lock);
    console_surface.show_keyboard = show_keyboard;
    console_surface.show_input = show_input;
    taskEXIT_CRITICAL(&console_surface_lock);
}

extern "C" void cyd_desktop_console_keyboard(int mode, int pressed_key) {
    taskENTER_CRITICAL(&console_surface_lock);
    console_surface.kb_mode = static_cast<uint8_t>(mode);
    console_surface.pressed_key = static_cast<int8_t>(pressed_key);
    taskEXIT_CRITICAL(&console_surface_lock);
}

extern "C" bool cyd_desktop_app_stop_requested(void) {
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    return snapshot.view != cyd::desktop::View::Scripts &&
        !(snapshot.view == cyd::desktop::View::Native &&
          cyd::desktop::native::has_flag(snapshot.foreground_app, cyd::desktop::native::kAppScriptHosted));
}

