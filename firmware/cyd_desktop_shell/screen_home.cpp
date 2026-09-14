#include "screen/views.h"
#include "screen/widgets.h"
#include "screen/touch.h"
#include "native_app.h"
#include "shell_state.h"

#include <algorithm>
#include <cstdio>

extern "C" bool cyd_desktop_script_launch_async(const char *app_id);

extern "C" bool cyd_desktop_apps_rescan_async(void);

namespace cyd::desktop::screen {

namespace {

uint8_t home_page = 0;

UiIcon launcher_ui_icon(cyd::desktop::native::LauncherIcon icon) {
    switch (icon) {
        case cyd::desktop::native::LauncherIcon::Movie: return UiIcon::Movie;
        case cyd::desktop::native::LauncherIcon::Game:
        case cyd::desktop::native::LauncherIcon::None:
        default: return UiIcon::Game;
    }
}

void home_icon(int x, int y, UiIcon icon, uint16_t color, const char *label) {
    card(x, y, 68, 64, 9, kCardAlt);
    fill_round_rect(x + 19, y + 7, 30, 30, 9, color);
    draw_icon(icon, x + 26, y + 14, kWhite);
    centered_text_fit(label, x + 34, y + 46, 60, kBlack);
}

void launcher_tile(int x, int y, UiIcon icon, uint16_t color, const char *label) {
    fill_round_rect(x, y, 88, 48, 8, kCardAlt);
    fill_round_rect(x + 8, y + 8, 30, 30, 8, color);
    draw_icon(icon, x + 15, y + 15, kWhite);
    text(label, x + 44, y + 21, kBlack);
}

void draw_start_menu() {
    card(55, 47, 210, 157, 13, kCard);
    draw_icon(UiIcon::Start, 70, 59, kWindowBlue);
    text("PINNED", 94, 63, kBlack);
    launcher_tile(66, 83, UiIcon::Clock, kWindowBlue, "Clock");
    launcher_tile(166, 83, UiIcon::Calendar, kGreen, "Calendar");
    launcher_tile(66, 139, UiIcon::Scripts, 0x9a5f, "Scripts");
    launcher_tile(166, 139, UiIcon::Settings, kDarkGray, "Settings");
    fill_rect(67, 193, 186, 1, kAccentSoft);
    ui_node("start.clock", "button", 66, 83, 88, 48, "Clock");
    ui_node("start.calendar", "button", 166, 83, 88, 48, "Calendar");
    ui_node("start.scripts", "button", 66, 139, 88, 48, "Scripts");
    ui_node("start.settings", "button", 166, 139, 88, 48, "Settings");
}

uint8_t home_total_pages(const cyd::desktop::Snapshot &snapshot) {
    if (snapshot.shortcut_count <= 2) return 1;
    return 1 + (snapshot.shortcut_count - 2 + 7) / 8;
}

void paint_home(const cyd::desktop::Snapshot &snapshot, bool include_start_menu = true) {
    wallpaper();
    fill_circle(27, 24, 13, kAccent);
    draw_icon(UiIcon::Start, 20, 17, kWhite);
    text("CYD DESKTOP", 49, 13, kWhite, 2);
    text("MICROPYTHON  /  READY", 50, 35, kAccentSoft);

    const uint8_t total_pages = home_total_pages(snapshot);
    if (home_page >= total_pages) home_page = 0;

    if (home_page == 0) {
        home_icon(10, 52, UiIcon::Clock, kWindowBlue, "CLOCK");
        home_icon(87, 52, UiIcon::Calendar, kGreen, "CALENDAR");
        home_icon(164, 52, UiIcon::Scripts, 0x9a5f, "SCRIPTS");
        home_icon(241, 52, UiIcon::Settings, kDarkGray, "SETTINGS");
        ui_node("home.clock", "button", 10, 52, 68, 64, "Clock");
        ui_node("home.calendar", "button", 87, 52, 68, 64, "Calendar");
        ui_node("home.scripts", "button", 164, 52, 68, 64, "Scripts");
        ui_node("home.settings", "button", 241, 52, 68, 64, "Settings");

        // Row 2 Slots 0 and 1: Only pinned shortcuts
        for (uint8_t slot = 0; slot < 2; ++slot) {
            if (slot < snapshot.shortcut_count) {
                const auto &shortcut = snapshot.shortcuts[slot];
                const int x = 10 + slot * 77;
                home_icon(x, 122, UiIcon::Scripts, kWindowBlue, shortcut.title);
                char node_id[41];
                std::snprintf(node_id, sizeof(node_id), "home.shortcut.%s", shortcut.id);
                ui_node(node_id, "button", x, 122, 68, 64, shortcut.title);
            }
        }
        // Slot 2: Native EDITOR (replaces MJP Movie)
        home_icon(164, 122, UiIcon::Editor, 0x051d, "EDITOR");
        ui_node("home.editor", "button", 164, 122, 68, 64, "Editor");
        if (const auto *launcher = cyd::desktop::native::home_launcher()) {
            home_icon(241, 122, launcher_ui_icon(launcher->launcher_icon), launcher->launcher_color,
                      launcher->launcher_label);
            char node_id[41];
            std::snprintf(node_id, sizeof(node_id), "home.native.%s", launcher->id);
            ui_node(node_id, "button", 241, 122, 68, 64, launcher->title);
        }
    } else {
        // Page 1+: Pinned shortcuts (indexes 2..)
        const uint8_t base_index = 2 + (home_page - 1) * 8;
        for (uint8_t slot = 0; slot < 8; ++slot) {
            const uint8_t sc_idx = base_index + slot;
            if (sc_idx >= snapshot.shortcut_count) break;
            const int col = slot % 4;
            const int row = slot / 4;
            const int x = 10 + col * 77;
            const int y = (row == 0) ? 52 : 122;
            const auto &shortcut = snapshot.shortcuts[sc_idx];
            home_icon(x, y, UiIcon::Scripts, kWindowBlue, shortcut.title);
            char node_id[41];
            std::snprintf(node_id, sizeof(node_id), "home.shortcut.%s", shortcut.id);
            ui_node(node_id, "button", x, y, 68, 64, shortcut.title);
        }
    }

    if (!calibrated) {
        fill_round_rect(82, 190, 156, 17, 8, kYellow);
        centered_text("TOUCH NEEDS CALIBRATION", 160, 195, kBlack);
    } else if (total_pages > 1) {
        fill_round_rect(84, 190, 32, 16, 4, kCardAlt);
        centered_text("<", 100, 192, kBlack);
        ui_node("home.page_prev", "button", 84, 190, 32, 16, "<");

        const int dot_start_x = 160 - ((total_pages - 1) * 12) / 2;
        for (uint8_t p = 0; p < total_pages; ++p) {
            const int dx = dot_start_x + p * 12;
            if (p == home_page) {
                fill_circle(dx, 198, 3, kAccent);
            } else {
                circle(dx, 198, 3, kAccentSoft);
            }
        }

        fill_round_rect(204, 190, 32, 16, 4, kCardAlt);
        centered_text(">", 220, 192, kBlack);
        ui_node("home.page_next", "button", 204, 190, 32, 16, ">");
    }
    taskbar();
    if (snapshot.start_open && include_start_menu) draw_start_menu();
}

}  // namespace

void render_home(const cyd::desktop::Snapshot &snapshot) {
    // Opening Start is an overlay transition. Recompose its old Home
    // background and final menu together inside each tile, then replace only
    // that finished band. This avoids exposing the former staged card style.
    if (snapshot.start_open) {
        constexpr Rect menu_bounds{55, 47, 212, 160};
        clear_clip();
        for (int y = menu_bounds.y; y < menu_bounds.y + menu_bounds.height;
             y += kTransitionTileRows) {
            const Rect band{menu_bounds.x, y, menu_bounds.width,
                std::min(kTransitionTileRows, menu_bounds.y + menu_bounds.height - y)};
            begin_transition_tile(band.y, band.height);
            ui_nodes_suppressed = y != menu_bounds.y;
            paint_home(snapshot, false);
            draw_start_menu();
            ui_nodes_suppressed = false;
            flush_transition_rect(band);
        }
        return;
    }
    render_in_transition_tiles([&snapshot]() { paint_home(snapshot); });
}

void home_touch_down(int x, int y, const cyd::desktop::Snapshot &snapshot) {
    if (snapshot.start_open) {
        if (x >= 66 && x < 154 && y >= 83 && y < 131)
            launcher_tile(66, 83, UiIcon::Clock, kAccent, "Clock");
        else if (x >= 166 && x < 254 && y >= 83 && y < 131)
            launcher_tile(166, 83, UiIcon::Calendar, kAccent, "Calendar");
        else if (x >= 66 && x < 154 && y >= 139 && y < 187)
            launcher_tile(66, 139, UiIcon::Scripts, kAccent, "Scripts");
        else if (x >= 166 && x < 254 && y >= 139 && y < 187)
            launcher_tile(166, 139, UiIcon::Settings, kAccent, "Settings");
        return;
    }
    if (y >= 58 && y < 124) {
        if (x >= 10 && x < 78) home_icon(10, 58, UiIcon::Clock, kAccent, "CLOCK");
        else if (x >= 87 && x < 155) home_icon(87, 58, UiIcon::Calendar, kAccent, "CALENDAR");
        else if (x >= 164 && x < 232) home_icon(164, 58, UiIcon::Scripts, kAccent, "SCRIPTS");
        else if (x >= 241 && x < 309) home_icon(241, 58, UiIcon::Settings, kAccent, "SETTINGS");
    }
}

void handle_home_touch(int x, int y, const cyd::desktop::Snapshot &snapshot) {
    auto &shell = cyd::desktop::shell_state();
    if (snapshot.start_open) {
        if (x >= 66 && x < 154 && y >= 83 && y < 131) {
            shell.dispatch(cyd::desktop::Action::LaunchClock); record_input("tap.clock");
        } else if (x >= 166 && x < 254 && y >= 83 && y < 131) {
            shell.dispatch(cyd::desktop::Action::LaunchCalendar); record_input("tap.calendar");
        } else if (x >= 66 && x < 154 && y >= 139 && y < 187) {
            scripts_view_reset();
            shell.dispatch(cyd::desktop::Action::LaunchScripts); cyd_desktop_apps_rescan_async(); record_input("tap.scripts");
        } else if (x >= 166 && x < 254 && y >= 139 && y < 187) {
            settings_view_reset();
            shell.dispatch(cyd::desktop::Action::LaunchSettings); record_input("tap.settings");
        }
    } else {
        const uint8_t total_pages = home_total_pages(snapshot);
        if (total_pages > 1 && y >= 186 && y < 210) {
            if (x >= 70 && x < 135) {
                home_page = (home_page > 0) ? (home_page - 1) : (total_pages - 1);
                record_input("tap.home.page_prev");
                render();
                return;
            } else if (x >= 185 && x < 250) {
                home_page = (home_page + 1 < total_pages) ? (home_page + 1) : 0;
                record_input("tap.home.page_next");
                render();
                return;
            }
        }
        if (home_page == 0) {
            if (y >= 50 && y < 118) {
                if (x >= 10 && x < 78) { shell.dispatch(cyd::desktop::Action::LaunchClock); record_input("tap.clock"); }
                else if (x >= 87 && x < 155) { shell.dispatch(cyd::desktop::Action::LaunchCalendar); record_input("tap.calendar"); }
                else if (x >= 164 && x < 232) { scripts_view_reset(); shell.dispatch(cyd::desktop::Action::LaunchScripts); cyd_desktop_apps_rescan_async(); record_input("tap.scripts"); }
                else if (x >= 241 && x < 309) { settings_view_reset(); shell.dispatch(cyd::desktop::Action::LaunchSettings); record_input("tap.settings"); }
            } else if (y >= 120 && y < 188) {
                if (x >= 164 && x < 232) {
                    // A fresh buffer, not a real file: opening /sd/wifi.json here
                    // made the first SAVE overwrite the Wi-Fi settings (D19).
                    editor_new();
                    shell.dispatch(cyd::desktop::Action::LaunchEditor);
                    record_input("tap.editor");
                    render();
                    return;
                }
                if (x >= 241 && x < 309) {
                    if (const auto *launcher = cyd::desktop::native::home_launcher()) {
                        shell.launch_native(launcher->id);
                        // Sized to record_input's 32-byte slot so nothing is cut twice.
                        char event[kInputEventCapacity];
                        std::snprintf(event, sizeof(event), "tap.native.%s", launcher->id);
                        record_input(event);
                        render();
                    }
                    return;
                }
                for (uint8_t slot = 0; slot < 2; ++slot) {
                    const int left = 10 + slot * 77;
                    if (x >= left && x < left + 68) {
                        if (slot < snapshot.shortcut_count && cyd_desktop_script_launch_async(snapshot.shortcuts[slot].app_id)) {
                            record_input("tap.shortcut");
                        }
                        return;
                    }
                }
            }
        } else {
            // Page 1+: Pinned shortcuts
            const uint8_t base_index = 2 + (home_page - 1) * 8;
            int row = -1;
            if (y >= 50 && y < 118) row = 0;
            else if (y >= 120 && y < 188) row = 1;
            if (row >= 0) {
                for (int col = 0; col < 4; ++col) {
                    const int left = 10 + col * 77;
                    if (x >= left && x < left + 68) {
                        const uint8_t sc_idx = base_index + row * 4 + col;
                        if (sc_idx < snapshot.shortcut_count && cyd_desktop_script_launch_async(snapshot.shortcuts[sc_idx].app_id)) {
                            record_input("tap.shortcut");
                        }
                        return;
                    }
                }
            }
        }
    }
    render();
}

void home_view_reset() { home_page = 0; }

}  // namespace cyd::desktop::screen
