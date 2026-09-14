#include "shell_state.h"

#include <cstdio>
#include <cstring>

namespace cyd::desktop {

namespace {
ShellState shared_shell;

const char *app_name(View view) {
    switch (view) {
        case View::Clock: return "clock";
        case View::Calendar: return "calendar";
        case View::Scripts: return "scripts";
        case View::Settings: return "settings";
        case View::Editor: return "editor";
        default: return "";
    }
}
}  // namespace

ShellState &shell_state() { return shared_shell; }

// snapshot() is on the per-frame render path and is called several times per
// frame, sometimes only to read `view`. Carrying the shortcut and app tables by
// value once pushed it to ~2 kB and the display visibly flashed. Readers that
// need the tables use copy_shortcuts / copy_apps instead.
static_assert(sizeof(Snapshot) <= 64,
              "Snapshot is on the per-frame path: keep it a handful of pointers");

Snapshot ShellState::snapshot() const {
    if (view_ == View::Scripts && script_app_[0] != '\0')
        return {view_, start_open_, script_app_, shortcuts_, shortcut_count_, apps_, app_count_};
    if (view_ == View::Native && native_app_[0] != '\0')
        return {view_, start_open_, native_app_, shortcuts_, shortcut_count_, apps_, app_count_};
    return {view_, start_open_, app_name(view_), shortcuts_, shortcut_count_, apps_, app_count_};
}

uint8_t ShellState::copy_shortcuts(Shortcut *out, uint8_t capacity) const {
    if (out == nullptr) return 0;
    const uint8_t count = shortcut_count_ < capacity ? shortcut_count_ : capacity;
    for (uint8_t index = 0; index < count; ++index) out[index] = shortcuts_[index];
    return count;
}

uint8_t ShellState::copy_apps(InstalledApp *out, uint8_t capacity) const {
    if (out == nullptr) return 0;
    const uint8_t count = app_count_ < capacity ? app_count_ : capacity;
    for (uint8_t index = 0; index < count; ++index) out[index] = apps_[index];
    return count;
}

bool ShellState::dispatch(Action action) {
    switch (action) {
        case Action::GoHome:
            reset();
            return true;
        case Action::OpenStart:
            if (view_ != View::Home) return false;
            start_open_ = true;
            return true;
        case Action::LaunchClock: view_ = View::Clock; break;
        case Action::LaunchCalendar: view_ = View::Calendar; break;
        case Action::LaunchScripts: view_ = View::Scripts; break;
        case Action::LaunchSettings: view_ = View::Settings; break;
        case Action::LaunchEditor: view_ = View::Editor; break;
    }
    start_open_ = false;
    return true;
}

bool ShellState::launch_script(const char *app_id) {
    if (app_id == nullptr || app_id[0] == '\0') return false;
    std::snprintf(script_app_, sizeof(script_app_), "%s", app_id);
    view_ = View::Scripts;
    start_open_ = false;
    return true;
}

bool ShellState::launch_native(const char *app_id) {
    if (app_id == nullptr || app_id[0] == '\0') return false;
    std::snprintf(native_app_, sizeof(native_app_), "%s", app_id);
    view_ = View::Native;
    start_open_ = false;
    return true;
}

void ShellState::shortcut_clear() {
    shortcut_count_ = 0;
    for (auto &shortcut : shortcuts_) shortcut = {};
}

bool ShellState::shortcut_set(const char *id, const char *app_id, const char *title) {
    if (id == nullptr || app_id == nullptr || title == nullptr || id[0] == '\0' || app_id[0] == '\0' || title[0] == '\0') return false;
    for (uint8_t index = 0; index < shortcut_count_; ++index) {
        if (std::strcmp(shortcuts_[index].id, id) != 0) continue;
        std::snprintf(shortcuts_[index].app_id, sizeof(shortcuts_[index].app_id), "%s", app_id);
        std::snprintf(shortcuts_[index].title, sizeof(shortcuts_[index].title), "%s", title);
        return true;
    }
    if (shortcut_count_ >= kMaxShortcuts) return false;
    auto &shortcut = shortcuts_[shortcut_count_++];
    std::snprintf(shortcut.id, sizeof(shortcut.id), "%s", id);
    std::snprintf(shortcut.app_id, sizeof(shortcut.app_id), "%s", app_id);
    std::snprintf(shortcut.title, sizeof(shortcut.title), "%s", title);
    return true;
}

bool ShellState::shortcut_remove(const char *id) {
    if (id == nullptr || id[0] == '\0') return false;
    for (uint8_t index = 0; index < shortcut_count_; ++index) {
        if (std::strcmp(shortcuts_[index].id, id) != 0) continue;
        for (uint8_t rest = index + 1; rest < shortcut_count_; ++rest) shortcuts_[rest - 1] = shortcuts_[rest];
        shortcuts_[--shortcut_count_] = {};
        return true;
    }
    return false;
}

uint8_t ShellState::shortcut_count() const { return shortcut_count_; }

void ShellState::app_catalog_clear() {
    app_count_ = 0;
    for (auto &app : apps_) app = {};
}

bool ShellState::app_catalog_add(const char *app_id, const char *title) {
    if (app_id == nullptr || title == nullptr || app_id[0] == '\0' || title[0] == '\0') return false;
    for (uint8_t index = 0; index < app_count_; ++index) {
        if (std::strcmp(apps_[index].app_id, app_id) != 0) continue;
        std::snprintf(apps_[index].title, sizeof(apps_[index].title), "%s", title);
        return true;
    }
    if (app_count_ >= kMaxInstalledApps) return false;
    auto &app = apps_[app_count_++];
    std::snprintf(app.app_id, sizeof(app.app_id), "%s", app_id);
    std::snprintf(app.title, sizeof(app.title), "%s", title);
    return true;
}

uint8_t ShellState::app_catalog_count() const { return app_count_; }

void ShellState::reset() {
    view_ = View::Home;
    start_open_ = false;
    script_app_[0] = '\0';
    native_app_[0] = '\0';
}

}  // namespace cyd::desktop
