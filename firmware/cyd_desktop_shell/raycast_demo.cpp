#include "native_app.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cyd::desktop::native {
namespace {

constexpr int kRayCount = 160;
constexpr int kSceneHeight = 188;
constexpr float kPlaneScale = 0.66f;
constexpr float kPi = 3.1415926535f;

constexpr uint8_t kWorld[12][12] = {
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,2,2,0,0,0,0,3,3,0,1},
    {1,0,2,0,0,0,1,0,0,3,0,1},
    {1,0,0,0,1,0,1,0,0,0,0,1},
    {1,0,0,0,1,0,0,0,4,4,0,1},
    {1,0,1,0,1,0,0,0,4,0,0,1},
    {1,0,1,0,0,0,2,0,0,0,0,1},
    {1,0,0,0,3,3,3,0,0,1,0,1},
    {1,0,0,0,0,0,0,0,0,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
};

struct RayColumn {
    int16_t top;
    int16_t bottom;
    uint16_t color;
    uint16_t pattern;
    float distance;
};

enum class Control : uint8_t { None, Left, Forward, Right, Fire };

struct DemoState {
    float x = 2.5f;
    float y = 5.5f;
    float angle = -0.35f;
    RayColumn rays[kRayCount]{};
    Control held = Control::None;
    bool enemy_alive = true;
    int enemy_screen_x = -100;
    int enemy_top = 0;
    int enemy_bottom = 0;
    float enemy_depth = 99.0f;
    uint8_t muzzle_frames = 0;
    uint8_t score = 0;
    uint16_t pulse = 0;
} state;

uint16_t wall_color(uint8_t wall, bool side) {
    static constexpr uint16_t colors[] = {
        rgb565(0, 0, 0), rgb565(40, 200, 255), rgb565(188, 68, 255),
        rgb565(255, 55, 132), rgb565(45, 240, 160),
    };
    uint16_t value = colors[std::min<uint8_t>(wall, 4)];
    if (!side) return value;
    const uint16_t red = (value >> 11) & 0x1f;
    const uint16_t green = (value >> 5) & 0x3f;
    const uint16_t blue = value & 0x1f;
    return static_cast<uint16_t>(((red * 5 / 8) << 11) | ((green * 5 / 8) << 5) | (blue * 5 / 8));
}

bool open_at(float x, float y) {
    const int map_x = static_cast<int>(x);
    const int map_y = static_cast<int>(y);
    return map_x >= 0 && map_x < 12 && map_y >= 0 && map_y < 12 && kWorld[map_y][map_x] == 0;
}

void prepare_frame() {
    const float direction_x = std::cos(state.angle);
    const float direction_y = std::sin(state.angle);
    const float plane_x = -direction_y * kPlaneScale;
    const float plane_y = direction_x * kPlaneScale;
    for (int ray = 0; ray < kRayCount; ++ray) {
        const float camera_x = 2.0f * ray / static_cast<float>(kRayCount - 1) - 1.0f;
        const float ray_x = direction_x + plane_x * camera_x;
        const float ray_y = direction_y + plane_y * camera_x;
        int map_x = static_cast<int>(state.x);
        int map_y = static_cast<int>(state.y);
        const float delta_x = std::fabs(ray_x) < 0.0001f ? 10000.0f : std::fabs(1.0f / ray_x);
        const float delta_y = std::fabs(ray_y) < 0.0001f ? 10000.0f : std::fabs(1.0f / ray_y);
        const int step_x = ray_x < 0 ? -1 : 1;
        const int step_y = ray_y < 0 ? -1 : 1;
        float side_x = ray_x < 0 ? (state.x - map_x) * delta_x : (map_x + 1.0f - state.x) * delta_x;
        float side_y = ray_y < 0 ? (state.y - map_y) * delta_y : (map_y + 1.0f - state.y) * delta_y;
        bool side = false;
        uint8_t wall = 1;
        for (int step = 0; step < 24; ++step) {
            if (side_x < side_y) {
                side_x += delta_x;
                map_x += step_x;
                side = false;
            } else {
                side_y += delta_y;
                map_y += step_y;
                side = true;
            }
            if (map_x < 0 || map_x >= 12 || map_y < 0 || map_y >= 12) break;
            wall = kWorld[map_y][map_x];
            if (wall != 0) break;
        }
        const float distance = std::max(0.15f, side ? side_y - delta_y : side_x - delta_x);
        const int wall_height = std::min(kSceneHeight * 2, static_cast<int>(kSceneHeight / distance));
        RayColumn &column = state.rays[ray];
        column.top = static_cast<int16_t>(std::max(0, (kSceneHeight - wall_height) / 2));
        column.bottom = static_cast<int16_t>(std::min(kSceneHeight - 1, (kSceneHeight + wall_height) / 2));
        column.color = wall_color(wall, side);
        column.pattern = static_cast<uint16_t>((map_x * 13 + map_y * 7 + ray) & 15);
        column.distance = distance;
    }

    const float enemy_x = 7.5f - state.x;
    const float enemy_y = 6.5f - state.y;
    const float determinant = plane_x * direction_y - direction_x * plane_y;
    const float inverse = std::fabs(determinant) < 0.001f ? 0.0f : 1.0f / determinant;
    const float transform_x = inverse * (direction_y * enemy_x - direction_x * enemy_y);
    const float transform_y = inverse * (-plane_y * enemy_x + plane_x * enemy_y);
    state.enemy_depth = transform_y;
    if (transform_y > 0.15f) {
        state.enemy_screen_x = static_cast<int>((kScreenWidth / 2) * (1.0f + transform_x / transform_y));
        const int size = std::min(100, static_cast<int>(kSceneHeight / transform_y));
        state.enemy_top = std::max(2, kSceneHeight / 2 - size / 2);
        state.enemy_bottom = std::min(kSceneHeight - 2, kSceneHeight / 2 + size / 2);
    } else {
        state.enemy_screen_x = -100;
    }
}

Control control_at(int x, int y) {
    if (y < 184) return Control::Fire;
    if (x < 70) return Control::Left;
    if (x < 145) return Control::Forward;
    if (x < 220) return Control::Right;
    return Control::Fire;
}

void fire() {
    state.muzzle_frames = 3;
    if (!state.enemy_alive || state.enemy_depth <= 0.15f) return;
    const int half_size = (state.enemy_bottom - state.enemy_top) / 2;
    if (std::abs(state.enemy_screen_x - kScreenWidth / 2) <= std::max(5, half_size / 2) &&
        state.enemy_depth < state.rays[kRayCount / 2].distance) {
        state.enemy_alive = false;
        ++state.score;
    }
}

void apply_control(Control control, float seconds) {
    if (control == Control::Left) state.angle -= 1.65f * seconds;
    if (control == Control::Right) state.angle += 1.65f * seconds;
    if (control != Control::Forward) return;
    const float next_x = state.x + std::cos(state.angle) * 1.55f * seconds;
    const float next_y = state.y + std::sin(state.angle) * 1.55f * seconds;
    if (open_at(next_x, state.y)) state.x = next_x;
    if (open_at(state.x, next_y)) state.y = next_y;
}

void enter() {
    state = {};
    state.x = 2.5f;
    state.y = 5.5f;
    state.angle = -0.35f;
    state.enemy_alive = true;
    state.enemy_screen_x = -100;
    state.enemy_depth = 99.0f;
    prepare_frame();
}

void leave() { state.held = Control::None; }

bool update(uint32_t delta_ms) {
    const float seconds = std::min<uint32_t>(delta_ms, 100) / 1000.0f;
    apply_control(state.held, seconds);
    if (state.muzzle_frames > 0) --state.muzzle_frames;
    ++state.pulse;
    prepare_frame();
    return true;
}

void input(const InputEvent &event) {
    const Control control = control_at(event.x, event.y);
    if (event.type == InputType::Down || event.type == InputType::Move) {
        if (control == Control::Fire) {
            if (state.held != Control::Fire) fire();
            state.held = Control::Fire;
        } else {
            state.held = control;
        }
    } else if (event.type == InputType::Up) {
        state.held = Control::None;
    } else if (event.type == InputType::Tap) {
        if (control == Control::Fire) fire();
        else apply_control(control, 0.16f);
        prepare_frame();
    }
}

uint16_t shade(uint16_t color, int numerator, int denominator) {
    return static_cast<uint16_t>((((color >> 11) & 0x1f) * numerator / denominator) << 11 |
        (((color >> 5) & 0x3f) * numerator / denominator) << 5 |
        ((color & 0x1f) * numerator / denominator));
}

void render_tile(const TileSurface &surface) {
    constexpr uint16_t sky_top = rgb565(4, 8, 30);
    constexpr uint16_t sky_low = rgb565(12, 25, 62);
    constexpr uint16_t floor_a = rgb565(15, 18, 32);
    constexpr uint16_t floor_b = rgb565(24, 20, 42);
    constexpr uint16_t hud = rgb565(8, 10, 24);
    constexpr uint16_t cyan = rgb565(40, 220, 255);
    constexpr uint16_t pink = rgb565(255, 55, 150);
    constexpr uint16_t white = rgb565(235, 245, 255);
    constexpr uint16_t orange = rgb565(255, 150, 35);
    for (int local_y = 0; local_y < surface.height; ++local_y) {
        const int y = surface.origin_y + local_y;
        uint16_t *row = surface.pixels + static_cast<size_t>(local_y) * surface.stride;
        for (int x = 0; x < surface.width; ++x) {
            uint16_t color = hud;
            if (y < kSceneHeight) {
                const RayColumn &column = state.rays[x / 2];
                if (y < column.top) {
                    color = y < kSceneHeight / 3 ? sky_top : sky_low;
                    if (((x + state.pulse / 2) % 73 == 0) && y < 62) color = white;
                } else if (y <= column.bottom) {
                    color = column.color;
                    if (((y - column.top + column.pattern) & 15) == 0 || (x & 31) == 0)
                        color = shade(color, 5, 8);
                } else {
                    color = (((x / 12) ^ (y / 8)) & 1) ? floor_a : floor_b;
                }

                if (state.enemy_alive && state.enemy_depth > 0.15f) {
                    const int sprite_half = (state.enemy_bottom - state.enemy_top) / 2;
                    const int dx = x - state.enemy_screen_x;
                    const int dy = y - (state.enemy_top + state.enemy_bottom) / 2;
                    if (std::abs(dx) < sprite_half / 2 && y >= state.enemy_top && y <= state.enemy_bottom &&
                        dx * dx + dy * dy < sprite_half * sprite_half &&
                        state.enemy_depth < column.distance) {
                        color = std::abs(dx) < std::max(2, sprite_half / 8) ? white : pink;
                    }
                }

                // Procedural plasma blaster, crosshair, and muzzle flash.
                const int weapon_dx = std::abs(x - 160);
                if (y > 151 && weapon_dx < (y - 142) * 2) color = weapon_dx < 12 ? cyan : rgb565(48, 52, 82);
                if ((std::abs(x - 160) <= 7 && y == 93) || (x == 160 && std::abs(y - 93) <= 7)) color = cyan;
                if (state.muzzle_frames > 0 && y > 137 && y < 166 && weapon_dx < (166 - y)) color = orange;
            } else {
                const bool selected = (x < 70 && state.held == Control::Left) ||
                    (x >= 70 && x < 145 && state.held == Control::Forward) ||
                    (x >= 145 && x < 220 && state.held == Control::Right) ||
                    (x >= 220 && state.held == Control::Fire);
                color = selected ? rgb565(35, 96, 142) : hud;
                if (y == 188 || x == 69 || x == 144 || x == 219) color = cyan;
                // Large glyph-like control marks avoid depending on shell fonts.
                if (y >= 196 && y <= 204) {
                    if (x >= 24 && x <= 43 && (x + y == 228 || x - y == -172)) color = white;
                    if (x >= 101 && x <= 113 && (y == 196 || std::abs(x - 107) <= 1)) color = white;
                    if (x >= 174 && x <= 193 && (x - y == -22 || x + y == 397)) color = white;
                    const int fire_dx = x - 270;
                    const int fire_dy = y - 200;
                    if (fire_dx * fire_dx + fire_dy * fire_dy >= 36 &&
                        fire_dx * fire_dx + fire_dy * fire_dy <= 72) color = pink;
                }
            }
            row[x] = byte_swap(color);
        }
    }
}

// The on-screen control strip drawn by render_tile, published so that
// desktop_ui_tree can describe it and clients can tap it by name.
void describe_ui(UiNodeFn emit) {
    emit("native.turn_left", "button", 0, 188, 70, 24, "Turn left");
    emit("native.forward", "button", 70, 188, 75, 24, "Forward");
    emit("native.turn_right", "button", 145, 188, 75, 24, "Turn right");
    emit("native.fire", "button", 220, 188, 100, 24, "Fire");
}

const NativeApp app{
    "neon3d", "NEON STRIKE 3D", 66,
    enter, leave, update, input, render_tile,
    kAppLaunchable, "NEON 3D", 0xf92f, LauncherIcon::Game, describe_ui,
};

}  // namespace

const NativeApp &raycast_demo_app() { return app; }

}  // namespace cyd::desktop::native
