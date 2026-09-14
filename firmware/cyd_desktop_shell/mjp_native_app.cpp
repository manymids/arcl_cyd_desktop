#include "native_app.h"

namespace cyd::desktop::native {
namespace {

void enter() {}
void leave() {}
bool update(uint32_t) { return false; }
void input(const InputEvent &) {}

void render_tile(const TileSurface &surface) {
    constexpr uint16_t black = rgb565(0, 0, 0);
    constexpr uint16_t navy = rgb565(5, 10, 28);
    constexpr uint16_t cyan = rgb565(30, 220, 255);
    constexpr uint16_t white = rgb565(235, 245, 255);
    surface.fill(black);
    for (int y = surface.origin_y; y < surface.origin_y + surface.height; ++y) {
        if (y >= 70 && y < 142) surface.hline(56, y, 208, navy);
        if (y == 70 || y == 141) surface.hline(56, y, 208, cyan);
        if (y >= 76 && y < 136 && ((y - 76) % 15) < 8) {
            surface.hline(62, y, 8, white);
            surface.hline(250, y, 8, white);
        }
        if (y >= 91 && y <= 121) {
            const int half = (y - 91) / 2;
            surface.hline(144 - half, y, half * 2 + 1, cyan);
        }
    }
}

// Not launchable by id: the MicroPython player enters it through
// cyd.video_begin() once it has opened a movie, and keeps driving it.
const NativeApp app{
    "mjpplayer", "MJP MOVIE", 1000,
    enter, leave, update, input, render_tile,
    kAppScriptHosted | kAppVideoSurface, nullptr, 0, LauncherIcon::None, nullptr,
};

}  // namespace

const NativeApp &mjp_player_app() { return app; }

}  // namespace cyd::desktop::native
