#include "native_app.h"

#include <cstring>

// Every native app is listed in this file and nowhere else.
//
// To add one: implement its NativeApp in its own .cpp (see
// docs/native-app-guide.md), declare the accessor below, add one line to the
// table, and list the .cpp in micropython.cmake. The shell learns what to do
// with the app from its flags, launcher fields and describe_ui - no shell code
// needs to know the id.

namespace cyd::desktop::native {

const NativeApp &raycast_demo_app();
const NativeApp &mjp_player_app();

namespace {

const NativeApp *const *table(std::size_t &count) {
    static const NativeApp *const apps[] = {
        &raycast_demo_app(),
        &mjp_player_app(),
    };
    count = sizeof(apps) / sizeof(apps[0]);
    return apps;
}

}  // namespace

std::size_t app_count() {
    std::size_t count = 0;
    table(count);
    return count;
}

const NativeApp *app_at(std::size_t index) {
    std::size_t count = 0;
    const NativeApp *const *apps = table(count);
    return index < count ? apps[index] : nullptr;
}

const NativeApp *find(const char *id) {
    if (id == nullptr || id[0] == '\0') return nullptr;
    for (std::size_t index = 0; index < app_count(); ++index) {
        const NativeApp *app = app_at(index);
        if (std::strcmp(app->id, id) == 0) return app;
    }
    return nullptr;
}

const NativeApp *find_with_flag(uint8_t flag) {
    for (std::size_t index = 0; index < app_count(); ++index) {
        const NativeApp *app = app_at(index);
        if ((app->flags & flag) != 0) return app;
    }
    return nullptr;
}

const NativeApp *home_launcher() {
    for (std::size_t index = 0; index < app_count(); ++index) {
        const NativeApp *app = app_at(index);
        if (app->launcher_label != nullptr && (app->flags & kAppLaunchable) != 0) return app;
    }
    return nullptr;
}

bool has_flag(const char *id, uint8_t flag) {
    const NativeApp *app = find(id);
    return app != nullptr && (app->flags & flag) != 0;
}

}  // namespace cyd::desktop::native
