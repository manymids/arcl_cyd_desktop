#pragma once

#include <cstdint>

namespace cyd::desktop {

enum class View : uint8_t { Home, Start, Clock, Calendar, Scripts, Settings, Native, Editor };
enum class Action : uint8_t { OpenStart, GoHome, LaunchClock, LaunchCalendar, LaunchScripts, LaunchSettings, LaunchEditor };

struct Shortcut {
    char id[25];
    char app_id[25];
    char title[13];
};

struct InstalledApp {
    char app_id[25];
    char title[13];
};

// Deliberately small: screen.cpp builds one on every frame, several times
// just to read `view`. Readers that walk the shortcut or app tables must not
// use these pointers from another task - see copy_shortcuts / copy_apps.
struct Snapshot {
    View view;
    bool start_open;
    const char *foreground_app;
    const Shortcut *shortcuts;
    uint8_t shortcut_count;
    const InstalledApp *apps;
    uint8_t app_count;
};

class ShellState {
  public:
    static constexpr uint8_t kMaxHomeIcons = 6;
    static constexpr uint8_t kMaxShortcuts = 18;
    static constexpr uint8_t kMaxInstalledApps = 24;

    Snapshot snapshot() const;

    // The MicroPython task adds and removes entries while the protocol task
    // serves desktop_shortcuts_list and desktop_installed_apps_list. Those two
    // must iterate a copy, not the live array: shortcut_remove shifts entries
    // down, so a concurrent walk can see one twice or miss one. Returns the
    // number written.
    uint8_t copy_shortcuts(Shortcut *out, uint8_t capacity) const;
    uint8_t copy_apps(InstalledApp *out, uint8_t capacity) const;
    bool dispatch(Action action);
    bool launch_script(const char *app_id);
    bool launch_native(const char *app_id);
    void shortcut_clear();
    bool shortcut_set(const char *id, const char *app_id, const char *title);
    bool shortcut_remove(const char *id);
    uint8_t shortcut_count() const;
    void app_catalog_clear();
    bool app_catalog_add(const char *app_id, const char *title);
    uint8_t app_catalog_count() const;
    void reset();

  private:
    View view_ = View::Home;
    bool start_open_ = false;
    char script_app_[25] = {};
    char native_app_[25] = {};
    Shortcut shortcuts_[kMaxShortcuts] = {};
    uint8_t shortcut_count_ = 0;
    InstalledApp apps_[kMaxInstalledApps] = {};
    uint8_t app_count_ = 0;
};

// Shared by the native display task and the optional MicroPython bridge.
ShellState &shell_state();

}  // namespace cyd::desktop
