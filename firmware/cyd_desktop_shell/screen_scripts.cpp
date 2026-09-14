#include "screen/views.h"
#include "screen/widgets.h"
#include "shell_state.h"

#include <cstdio>
#include <cstring>

extern "C" bool cyd_desktop_script_launch_async(const char *app_id);
extern "C" bool cyd_desktop_package_submit(int operation, const char *app_id, const char *filename,
    const char *payload, uint32_t expected_size, const char *sha256, char *error, size_t error_capacity);

namespace cyd::desktop::screen {

namespace {

bool scripts_detail_page = false;
bool scripts_delete_armed = false;
uint8_t scripts_page = 0;
uint8_t scripts_selected = 0;
char native_notice[49] = {};

int shortcut_for_app(const cyd::desktop::Snapshot &snapshot, const char *app_id) {
    for (uint8_t index = 0; index < snapshot.shortcut_count; ++index)
        if (std::strcmp(snapshot.shortcuts[index].app_id, app_id) == 0) return index;
    return -1;
}

void paint_scripts(const cyd::desktop::Snapshot &snapshot) {
    if (snapshot.app_count == 0) {
        scripts_detail_page = false;
        scripts_page = 0;
    } else if (scripts_selected >= snapshot.app_count) {
        scripts_selected = snapshot.app_count - 1;
    }

    if (scripts_detail_page && snapshot.app_count > 0) {
        const auto &app = snapshot.apps[scripts_selected];
        native_page_header(UiIcon::Scripts, "App details", "MICROPYTHON PACKAGE");
        card(18, 51, 284, 79, 10, kCardAlt);
        fill_round_rect(31, 64, 38, 38, 10, kWindowBlue);
        draw_icon(UiIcon::Scripts, 42, 75, kWhite);
        text(app.title, 82, 62, kBlack, 2);
        text(app.app_id, 82, 83, kDarkGray);
        text("MAIN.PY READY", 82, 99, kGreen);
        text(shortcut_for_app(snapshot, app.app_id) >= 0 ? "PINNED TO HOME" : "NOT PINNED", 82, 113, kWindowBlue);
        native_button(18, 141, 86, 31, "RUN");
        native_button(117, 141, 86, 31,
                      shortcut_for_app(snapshot, app.app_id) >= 0 ? "UNPIN" : "PIN");
        native_button(216, 141, 86, 31, scripts_delete_armed ? "CONFIRM" : "DELETE", true);
        native_button(110, 178, 100, 22, "< LIST");
        ui_node("scripts.detail", "dialog", 18, 51, 284, 79, app.title, false);
        ui_node("scripts.run", "button", 18, 141, 86, 31, "Run");
        ui_node("scripts.pin", "button", 117, 141, 86, 31,
                shortcut_for_app(snapshot, app.app_id) >= 0 ? "Unpin" : "Pin");
        ui_node("scripts.delete", "button", 216, 141, 86, 31,
                scripts_delete_armed ? "Confirm delete" : "Delete");
        ui_node("scripts.list", "button", 110, 178, 100, 22, "Back to list");
        if (native_notice[0] != '\0') centered_text_fit(native_notice, 160, 202, 282, kDarkGray);
        taskbar();
        return;
    }

    native_page_header(UiIcon::Scripts, "Scripts", "INSTALLED APPS ON SD");
    constexpr uint8_t per_page = 3;
    const uint8_t pages = snapshot.app_count == 0 ? 1 : (snapshot.app_count + per_page - 1) / per_page;
    if (scripts_page >= pages) scripts_page = pages - 1;
    const uint8_t first = scripts_page * per_page;
    if (snapshot.app_count == 0) {
        card(18, 65, 284, 77, 10, kCardAlt);
        centered_text("NO APPS FOUND", 160, 83, kBlack, 2);
        centered_text("UPLOAD MAIN.PY TO /SD/APPS", 160, 113, kDarkGray);
    } else {
        for (uint8_t row = 0; row < per_page && first + row < snapshot.app_count; ++row) {
            const auto &app = snapshot.apps[first + row];
            const int y = 52 + row * 41;
            card(18, y, 284, 34, 8, kCardAlt);
            fill_round_rect(27, y + 6, 23, 23, 6, kWindowBlue);
            draw_icon(UiIcon::Scripts, 31, y + 9, kWhite);
            text(app.title, 59, y + 7, kBlack);
            text(app.app_id, 59, y + 19, kDarkGray);
            native_button(245, y + 5, 48, 24, "RUN");
            char detail_id[41];
            char run_id[41];
            std::snprintf(detail_id, sizeof(detail_id), "scripts.app.%s", app.app_id);
            std::snprintf(run_id, sizeof(run_id), "scripts.run.%s", app.app_id);
            ui_node(detail_id, "button", 18, y, 220, 34, app.title);
            ui_node(run_id, "button", 245, y + 5, 48, 24, "Run");
        }
    }
    if (scripts_page > 0) {
        native_button(18, 178, 58, 23, "< PREV");
        ui_node("scripts.prev", "button", 18, 178, 58, 23, "Previous page");
    }
    if (scripts_page + 1 < pages) {
        native_button(244, 178, 58, 23, "NEXT >");
        ui_node("scripts.next", "button", 244, 178, 58, 23, "Next page");
    }
    char page_text[24];
    std::snprintf(page_text, sizeof(page_text), "%u APPS  %u/%u", snapshot.app_count, scripts_page + 1, pages);
    centered_text(page_text, 160, 185, kDarkGray);
    if (native_notice[0] != '\0') centered_text_fit(native_notice, 160, 201, 282, kDarkGray);
    taskbar();
}

void set_native_notice(const char *value) {
    std::snprintf(native_notice, sizeof(native_notice), "%s", value == nullptr ? "" : value);
}

void launch_catalog_app(const cyd::desktop::Snapshot &snapshot, uint8_t index) {
    if (index >= snapshot.app_count) return;
    if (cyd_desktop_script_launch_async(snapshot.apps[index].app_id)) {
        set_native_notice("STARTING APP...");
        record_input("tap.app.run");
    } else {
        set_native_notice("RUNTIME BUSY");
    }
}

void toggle_catalog_shortcut(const cyd::desktop::Snapshot &snapshot, uint8_t index) {
    if (index >= snapshot.app_count) return;
    const auto &app = snapshot.apps[index];
    const int shortcut_index = shortcut_for_app(snapshot, app.app_id);
    char error[80] = {};
    const bool ok = shortcut_index >= 0
        ? cyd_desktop_package_submit(6, snapshot.shortcuts[shortcut_index].id, "", "", 0, "", error, sizeof(error))
        : cyd_desktop_package_submit(4, app.app_id, app.app_id, app.title, 0, "", error, sizeof(error));
    set_native_notice(ok ? (shortcut_index >= 0 ? "REMOVED FROM HOME" : "PINNED TO HOME") : error);
    record_input(shortcut_index >= 0 ? "tap.app.unpin" : "tap.app.pin");
}

void delete_catalog_app(const cyd::desktop::Snapshot &snapshot, uint8_t index) {
    if (index >= snapshot.app_count) return;
    if (!scripts_delete_armed) {
        scripts_delete_armed = true;
        set_native_notice("TAP CONFIRM TO DELETE");
        return;
    }
    char error[80] = {};
    const bool ok = cyd_desktop_package_submit(7, snapshot.apps[index].app_id, "", "", 0, "", error, sizeof(error));
    scripts_delete_armed = false;
    scripts_detail_page = !ok;
    scripts_page = 0;
    scripts_selected = 0;
    set_native_notice(ok ? "APP DELETED" : error);
    record_input("tap.app.delete");
}

}  // namespace

void render_scripts(const cyd::desktop::Snapshot &snapshot) {
    render_in_transition_tiles([&snapshot]() { paint_scripts(snapshot); });
}

void handle_scripts_touch(int x, int y, const cyd::desktop::Snapshot &snapshot) {
    constexpr uint8_t per_page = 3;
    if (scripts_detail_page && snapshot.app_count > 0) {
        if (y >= 141 && y < 175) {
            if (x >= 18 && x < 105) launch_catalog_app(snapshot, scripts_selected);
            else if (x >= 117 && x < 204) toggle_catalog_shortcut(snapshot, scripts_selected);
            else if (x >= 216 && x < 303) delete_catalog_app(snapshot, scripts_selected);
        } else if (x >= 105 && x < 215 && y >= 176 && y < 204) {
            scripts_detail_page = false;
            scripts_delete_armed = false;
            set_native_notice("");
        }
        render();
        return;
    }
    if (y >= 52 && y < 170) {
        const uint8_t row = static_cast<uint8_t>((y - 52) / 41);
        if ((y - 52) % 41 < 34) {
            const uint8_t index = scripts_page * per_page + row;
            if (index < snapshot.app_count) {
                if (x >= 235) launch_catalog_app(snapshot, index);
                else {
                    scripts_selected = index;
                    scripts_detail_page = true;
                    scripts_delete_armed = false;
                    set_native_notice("");
                    record_input("tap.app.details");
                }
            }
        }
    } else if (y >= 176 && y < 205) {
        if (x < 90 && scripts_page > 0) --scripts_page;
        else if (x > 230 && (scripts_page + 1) * per_page < snapshot.app_count) ++scripts_page;
    }
    render();
}

void scripts_view_reset() {
    scripts_detail_page = false;
    scripts_page = 0;
    set_native_notice("");
}

}  // namespace cyd::desktop::screen
