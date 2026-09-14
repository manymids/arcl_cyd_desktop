#include "native_app.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

using namespace cyd::desktop::native;

struct Node {
    std::string id;
    std::string role;
    int x, y, width, height;
};

std::vector<Node> emitted;

void collect(const char *id, const char *role, int x, int y, int width, int height, const char *) {
    emitted.push_back({id, role, x, y, width, height});
}

void lifecycle() {
    assert(!active());
    assert(!launch("missing"));
    assert(launch("neon3d"));
    assert(active());
    assert(current() != nullptr);
    assert(current()->target_frame_ms > 0);
    assert(update(66));
    input({InputType::Tap, 270, 200});

    uint16_t pixels[kScreenWidth * 16] = {};
    const TileSurface top{pixels, kScreenWidth, 16, kScreenWidth, 0};
    render_tile(top);
    bool changed = false;
    for (uint16_t pixel : pixels) changed = changed || pixel != 0;
    assert(changed);

    const TileSurface controls{pixels, kScreenWidth, 16, kScreenWidth, 196};
    render_tile(controls);
    note_frame(80);
    const auto result = metrics();
    assert(result.frames == 1);
    assert(result.last_frame_ms == 80);
    leave();
    assert(!active());
}

// Every registered app is fully described: the shell relies on these fields
// instead of knowing app ids.
void registry_is_well_formed() {
    assert(app_count() >= 2);
    assert(app_at(app_count()) == nullptr);
    for (std::size_t i = 0; i < app_count(); ++i) {
        const NativeApp *app = app_at(i);
        assert(app != nullptr);
        assert(app->id != nullptr && app->id[0] != '\0' && std::strlen(app->id) <= 24);
        assert(app->title != nullptr && app->title[0] != '\0');
        assert(app->enter && app->leave && app->update && app->input && app->render_tile);
        assert(find(app->id) == app);
        for (std::size_t j = i + 1; j < app_count(); ++j) {
            assert(std::strcmp(app->id, app_at(j)->id) != 0);  // ids are unique
        }
        if (app->launcher_label != nullptr) {
            assert(std::strlen(app->launcher_label) <= 8);  // fits the 68 px Home tile
        }
    }
    assert(find(nullptr) == nullptr);
    assert(find("") == nullptr);
    assert(find("missing") == nullptr);
}

void flags_drive_shell_behaviour() {
    // The Home slot is the launchable app that asked for one.
    const NativeApp *launcher = home_launcher();
    assert(launcher != nullptr);
    assert((launcher->flags & kAppLaunchable) != 0);
    assert(std::strcmp(launcher->id, "neon3d") == 0);

    // cyd.video_begin() resolves its target by flag, not by name.
    const NativeApp *video = find_with_flag(kAppVideoSurface);
    assert(video != nullptr);
    assert(std::strcmp(video->id, "mjpplayer") == 0);
    assert(has_flag("mjpplayer", kAppScriptHosted));

    // The video surface is entered by the player script, never launched by id;
    // letting desktop_launch start it would show an empty player.
    assert(!has_flag("mjpplayer", kAppLaunchable));
    assert(has_flag("neon3d", kAppLaunchable));
    assert(!has_flag("neon3d", kAppScriptHosted));
    assert(!has_flag("missing", kAppLaunchable));
}

void apps_describe_their_own_controls() {
    emitted.clear();
    const NativeApp *game = find("neon3d");
    assert(game->describe_ui != nullptr);
    game->describe_ui(collect);
    assert(emitted.size() == 4);
    for (const Node &node : emitted) {
        assert(node.id.rfind("native.", 0) == 0);
        assert(node.role == "button");
        assert(node.x >= 0 && node.y >= 0);
        assert(node.x + node.width <= kScreenWidth);
        assert(node.y + node.height <= kContentHeight);
    }
    assert(find("mjpplayer")->describe_ui == nullptr);
}

}  // namespace

int main() {
    lifecycle();
    registry_is_well_formed();
    flags_drive_shell_behaviour();
    apps_describe_their_own_controls();
    return 0;
}
