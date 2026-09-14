#include "bt_keyboard.h"
#include "screen/views.h"
#include "screen/widgets.h"
#include "screen/time_util.h"
#include "screen/touch.h"

extern "C" {
#include "driver/ledc.h"
#include "esp_heap_caps.h"
#include "nvs.h"
}

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <sys/time.h>

extern "C" bool cyd_desktop_sd_mounted(void);
extern "C" int cyd_desktop_sd_last_error(void);

namespace cyd::desktop::screen {

namespace {

constexpr uint32_t kSettingsMagic = 0x43594453;

struct UserSettings {
    uint32_t magic;
    uint8_t brightness;
    uint8_t theme;
    uint8_t animation;
    uint8_t keyboard_layout;  // kKeyboardLayoutJis (0, also older blobs) or kKeyboardLayoutUs
};

UserSettings user_settings{kSettingsMagic, 2, 0, 1, kKeyboardLayoutJis};
bool backlight_pwm_ready = false;
bool settings_system_page = false;
bool settings_rtc_page = false;
bool settings_wireless_page = false;
bool settings_keyboard_page = false;
uint32_t wireless_painted_generation = 0;
// The radio mode tapped on the Wireless page and awaiting confirmation, or -1.
int wireless_pending = -1;
struct RtcEditState {
    int year;
    int month;
    int day;
    int hour;
    int min;
};
RtcEditState rtc_edit{2026, 9, 11, 12, 0};

void init_rtc_edit() {
    std::tm t{};
    if (current_time(&t)) {
        rtc_edit.year = t.tm_year + 1900;
        rtc_edit.month = t.tm_mon + 1;
        rtc_edit.day = t.tm_mday;
        rtc_edit.hour = t.tm_hour;
        rtc_edit.min = t.tm_min;
    } else {
        rtc_edit = {2026, 9, 11, 12, 0};
    }
}

void apply_rtc_time() {
    std::tm t{};
    t.tm_year = rtc_edit.year - 1900;
    t.tm_mon = rtc_edit.month - 1;
    t.tm_mday = rtc_edit.day;
    t.tm_hour = rtc_edit.hour;
    t.tm_min = rtc_edit.min;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    const std::time_t epoch = std::mktime(&t);
    if (epoch >= 0) {
        struct timeval tv;
        tv.tv_sec = epoch;
        tv.tv_usec = 0;
        settimeofday(&tv, nullptr);
    }
}

const char *brightness_name() {
    static const char *names[] = {"35%", "65%", "100%"};
    return names[std::min<uint8_t>(user_settings.brightness, 2)];
}

const char *theme_name() {
    static const char *names[] = {"BLUE", "VIOLET", "TEAL"};
    return names[std::min<uint8_t>(user_settings.theme, 2)];
}

const char *animation_name() {
    static const char *names[] = {"OFF", "FAST", "SMOOTH"};
    return names[std::min<uint8_t>(user_settings.animation, 2)];
}

void apply_brightness() {
    if (!backlight_pwm_ready) return;
    static const uint32_t duty[] = {358, 665, 1023};
    const uint32_t value = duty[std::min<uint8_t>(user_settings.brightness, 2)];
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, value);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

bool save_user_settings() {
    nvs_handle_t handle;
    if (nvs_open("cyd_desktop", NVS_READWRITE, &handle) != ESP_OK) return false;
    const esp_err_t result = nvs_set_blob(handle, "ui", &user_settings, sizeof(user_settings));
    if (result == ESP_OK) nvs_commit(handle);
    nvs_close(handle);
    return result == ESP_OK;
}

// Six rows fill the card from y 45 to 199.
constexpr int kRowTop = 45;
constexpr int kRowPitch = 26;
constexpr int kRowHeight = 24;

int row_y(int row) { return kRowTop + row * kRowPitch; }

void setting_row(int row, const char *label, const char *description, const char *value) {
    const int y = row_y(row);
    card(18, y, 284, kRowHeight, 8, kCardAlt);
    text(label, 31, y + 4, kBlack);
    text(description, 31, y + 14, kDarkGray);
    fill_round_rect(225, y + 3, 65, 18, 9, kAccentSoft);
    centered_text_fit(value, 257, y + 9, 57, kWindowBlue);
}

const char *radio_label(int mode) {
    static const char *names[] = {"OFF", "WI-FI", "BLUETOOTH"};
    return names[mode >= 0 && mode <= 2 ? mode : 0];
}

constexpr int kModeButtonX[3] = {18, 116, 214};
constexpr int kModeButtonY = 50;
constexpr int kModeButtonWidth = 88;
constexpr int kModeButtonHeight = 30;

void paint_settings_wireless() {
    native_page_header(UiIcon::Settings, "Wireless", "WI-FI OR BLUETOOTH, NOT BOTH");
    const int running = cyd_desktop_radio_mode();
    const int next = cyd_desktop_radio_next_mode();
    const int selected = wireless_pending >= 0 ? wireless_pending : next;
    static const char *const ids[] = {"settings.wireless.off", "settings.wireless.wifi", "settings.wireless.bluetooth"};
    for (int mode = 0; mode < 3; ++mode) {
        const int x = kModeButtonX[mode];
        if (mode == selected) {
            native_button(x, kModeButtonY, kModeButtonWidth, kModeButtonHeight, radio_label(mode));
        } else {
            card(x, kModeButtonY, kModeButtonWidth, kModeButtonHeight, 7, kCardAlt);
            centered_text_fit(radio_label(mode), x + kModeButtonWidth / 2, kModeButtonY + 12, kModeButtonWidth - 8, kBlack);
        }
        ui_node(ids[mode], "button", x, kModeButtonY, kModeButtonWidth, kModeButtonHeight, radio_label(mode));
    }

    card(18, 90, 284, 62, 8, kCardAlt);
    char line_text[48];
    if (wireless_pending >= 0) {
        std::snprintf(line_text, sizeof(line_text), "SWITCH TO %s?", radio_label(wireless_pending));
        text(line_text, 31, 100, kBlack);
        text("THE DEVICE RESTARTS TO APPLY IT.", 31, 116, kDarkGray);
        text("UNSAVED EDITOR TEXT IS LOST.", 31, 132, kDarkGray);
        native_button(18, 162, 135, 31, "CANCEL");
        native_button(167, 162, 135, 31, "RESTART", true);
        ui_node("settings.wireless.cancel", "button", 18, 162, 135, 31, "Cancel");
        ui_node("settings.wireless.restart", "button", 167, 162, 135, 31, "Restart");
    } else {
        std::snprintf(line_text, sizeof(line_text), "RUNNING    %s", radio_label(running));
        text(line_text, 31, 100, kBlack);
        std::snprintf(line_text, sizeof(line_text), "NEXT BOOT  %s", radio_label(next));
        text(line_text, 31, 116, running == next ? kBlack : kRed);
        cyd_bt_snapshot_t bt;
        wireless_painted_generation = cyd_desktop_bt_generation();
        cyd_desktop_bt_snapshot(&bt);
        const char *bluetooth_note = bt.connected ? "KEYBOARD CONNECTED"
                                     : (bt.bonded ? "KEYBOARD NOT CONNECTED" : "NO KEYBOARD PAIRED");
        const char *const notes[] = {"WI-FI AND BLUETOOTH ARE OFF", "APPS CAN USE WI-FI", bluetooth_note};
        text(running == next ? notes[running] : "RESTART TO APPLY", 31, 132, running == next ? kDarkGray : kRed);
        native_button(18, 162, 135, 31, "< SETTINGS");
        ui_node("settings.wireless.back", "button", 18, 162, 135, 31, "Back to settings");
        if (running != next) {
            native_button(167, 162, 135, 31, "RESTART", true);
            ui_node("settings.wireless.restart", "button", 167, 162, 135, 31, "Restart");
        } else if (running == CYD_RADIO_BLUETOOTH) {
            native_button(167, 162, 135, 31, "KEYBOARD >");
            ui_node("settings.wireless.keyboard", "button", 167, 162, 135, 31, "Keyboard pairing");
        }
    }
    taskbar();
}

void handle_wireless_touch(int x, int y) {
    const int running = cyd_desktop_radio_mode();
    const int next = cyd_desktop_radio_next_mode();
    if (y >= kModeButtonY - 4 && y < kModeButtonY + kModeButtonHeight + 4) {
        const int mode = x < 111 ? 0 : (x < 209 ? 1 : 2);
        // Choosing the mode that is running and stored needs no restart;
        // anything else asks first.
        wireless_pending = mode == next && next == running ? -1 : mode;
        record_input("tap.settings.wireless_mode");
        render();
        return;
    }
    if (y < 158 || y >= 200) return;
    if (x >= 160 && (wireless_pending >= 0 || running != next)) {
        const int mode = wireless_pending >= 0 ? wireless_pending : next;
        record_input("tap.settings.wireless_restart");
        wireless_pending = -1;
        if (cyd_desktop_radio_set_next_mode(mode)) cyd_desktop_radio_restart();
        render();
        return;
    }
    if (x >= 160 && wireless_pending < 0 && running == CYD_RADIO_BLUETOOTH) {
        bluetooth_page_enter();
        settings_keyboard_page = true;
        record_input("tap.settings.keyboard");
        render();
        return;
    }
    if (x < 160) {
        if (wireless_pending >= 0) {
            wireless_pending = -1;
            record_input("tap.settings.wireless_cancel");
        } else {
            settings_wireless_page = false;
            record_input("tap.settings.back");
        }
        render();
    }
}

void paint_settings_rtc() {
    native_page_header(UiIcon::Clock, "Date & Time", "ADJUST RTC CLOCK");

    static const struct {
        const char *label;
        int x;
    } cols[5] = {
        {"YEAR", 16},
        {"MONTH", 75},
        {"DAY", 134},
        {"HOUR", 193},
        {"MIN", 252},
    };

    char val_str[16];
    for (int i = 0; i < 5; ++i) {
        const int col_x = cols[i].x;
        centered_text_fit(cols[i].label, col_x + 26, 48, 50, kDarkGray);
        native_button(col_x, 62, 52, 28, "+");
        card(col_x, 94, 52, 28, 6, kCardAlt);

        int val = 0;
        if (i == 0) val = rtc_edit.year;
        else if (i == 1) val = rtc_edit.month;
        else if (i == 2) val = rtc_edit.day;
        else if (i == 3) val = rtc_edit.hour;
        else val = rtc_edit.min;

        if (i == 0) {
            std::snprintf(val_str, sizeof(val_str), "%04d", val);
        } else {
            std::snprintf(val_str, sizeof(val_str), "%02d", val);
        }
        centered_text_fit(val_str, col_x + 26, 94 + (28 - 7) / 2, 48, kBlack);

        native_button(col_x, 126, 52, 28, "-");
    }

    native_button(18, 164, 135, 32, "CANCEL");
    native_button(167, 164, 135, 32, "SAVE & SET");

    ui_node("settings.rtc.year_up", "button", 16, 62, 52, 28, "+");
    ui_node("settings.rtc.year_down", "button", 16, 126, 52, 28, "-");
    ui_node("settings.rtc.month_up", "button", 75, 62, 52, 28, "+");
    ui_node("settings.rtc.month_down", "button", 75, 126, 52, 28, "-");
    ui_node("settings.rtc.day_up", "button", 134, 62, 52, 28, "+");
    ui_node("settings.rtc.day_down", "button", 134, 126, 52, 28, "-");
    ui_node("settings.rtc.hour_up", "button", 193, 62, 52, 28, "+");
    ui_node("settings.rtc.hour_down", "button", 193, 126, 52, 28, "-");
    ui_node("settings.rtc.min_up", "button", 252, 62, 52, 28, "+");
    ui_node("settings.rtc.min_down", "button", 252, 126, 52, 28, "-");
    ui_node("settings.rtc.cancel", "button", 18, 164, 135, 32, "Cancel");
    ui_node("settings.rtc.save", "button", 167, 164, 135, 32, "Save and set");

    taskbar();
}

void paint_settings() {
    if (settings_rtc_page) {
        paint_settings_rtc();
        return;
    }
    if (settings_keyboard_page) {
        paint_bluetooth_page();
        return;
    }
    if (settings_wireless_page) {
        paint_settings_wireless();
        return;
    }
    if (!settings_system_page) {
        native_page_header(UiIcon::Settings, "Settings", "TAP A ROW TO CHANGE");
        setting_row(0, "BRIGHTNESS", "BACKLIGHT LEVEL", brightness_name());
        setting_row(1, "THEME", "ACCENT PALETTE", theme_name());
        setting_row(2, "MOTION", "START ANIMATION", animation_name());
        char time_buf[16] = "--:--";
        std::tm t{};
        if (current_time(&t)) {
            std::snprintf(time_buf, sizeof(time_buf), "%02d:%02d", t.tm_hour, t.tm_min);
        }
        setting_row(3, "DATE & TIME", "SET RTC CLOCK", time_buf);
        const char *radio = radio_label(cyd_desktop_radio_mode());
        setting_row(4, "WIRELESS", "WI-FI OR BLUETOOTH", radio);
        setting_row(5, "SYSTEM", "DEVICE AND STORAGE", "OPEN >");
        ui_node("settings.brightness", "button", 18, row_y(0), 284, kRowHeight, brightness_name());
        ui_node("settings.theme", "button", 18, row_y(1), 284, kRowHeight, theme_name());
        ui_node("settings.motion", "button", 18, row_y(2), 284, kRowHeight, animation_name());
        ui_node("settings.rtc", "button", 18, row_y(3), 284, kRowHeight, time_buf);
        ui_node("settings.wireless", "button", 18, row_y(4), 284, kRowHeight, radio);
        ui_node("settings.system", "button", 18, row_y(5), 284, kRowHeight, "Open system status");
        taskbar();
        return;
    }

    native_page_header(UiIcon::Settings, "System", "DEVICE STATUS");
    card(18, 51, 284, 99, 10, kCardAlt);
    char line_text[48];
    text("ESP32-D0WD-V3  /  240MHZ", 31, 63, kBlack);
    std::snprintf(line_text, sizeof(line_text), "HEAP FREE  %u KB", static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024));
    text(line_text, 31, 79, kBlack);
    text("FLASH      4 MB", 31, 95, kBlack);
    std::snprintf(line_text, sizeof(line_text), "SD CARD    %s  ERR %d", cyd_desktop_sd_mounted() ? "MOUNTED" : "OFFLINE", cyd_desktop_sd_last_error());
    text(line_text, 31, 111, cyd_desktop_sd_mounted() ? kGreen : kRed);
    std::snprintf(line_text, sizeof(line_text), "TOUCH      %s", calibrated ? "CALIBRATED" : "NEEDS SETUP");
    text(line_text, 31, 127, calibrated ? kGreen : kRed);
    native_button(18, 162, 135, 31, "CALIBRATE");
    native_button(167, 162, 135, 31, "< SETTINGS");
    ui_node("settings.calibrate", "button", 18, 162, 135, 31, "Calibrate touch");
    ui_node("settings.back", "button", 167, 162, 135, 31, "Back to settings");
    taskbar();
}

}  // namespace

void init_backlight_pwm() {
    ledc_timer_config_t timer{};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    timer.timer_num = LEDC_TIMER_0;
    timer.freq_hz = 5000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    if (ledc_timer_config(&timer) != ESP_OK) return;
    ledc_channel_config_t channel{};
    channel.gpio_num = kBacklight;
    channel.speed_mode = LEDC_LOW_SPEED_MODE;
    channel.channel = LEDC_CHANNEL_0;
    channel.intr_type = LEDC_INTR_DISABLE;
    channel.timer_sel = LEDC_TIMER_0;
    channel.duty = 1023;
    channel.hpoint = 0;
    if (ledc_channel_config(&channel) != ESP_OK) return;
    backlight_pwm_ready = true;
    apply_brightness();
}

void load_user_settings() {
    UserSettings loaded{};
    nvs_handle_t handle;
    if (nvs_open("cyd_desktop", NVS_READONLY, &handle) == ESP_OK) {
        size_t length = sizeof(loaded);
        const esp_err_t result = nvs_get_blob(handle, "ui", &loaded, &length);
        nvs_close(handle);
        if (result == ESP_OK && length == sizeof(loaded) && loaded.magic == kSettingsMagic &&
            loaded.brightness <= 2 && loaded.theme <= 2 && loaded.animation <= 2) {
            user_settings = loaded;
            if (user_settings.keyboard_layout > kKeyboardLayoutUs) user_settings.keyboard_layout = kKeyboardLayoutJis;
        }
    }
    apply_theme(user_settings.theme);
}

void render_settings() {
    if (settings_keyboard_page) bluetooth_page_prepare();
    render_in_transition_tiles([]() { paint_settings(); });
}

void settings_poll() {
    if (settings_keyboard_page) {
        if (bluetooth_page_changed()) redraw_requested = true;
    } else if (settings_wireless_page && cyd_desktop_radio_mode() == CYD_RADIO_BLUETOOTH) {
        if (cyd_desktop_bt_generation() != wireless_painted_generation) redraw_requested = true;
    }
}

uint8_t keyboard_layout() {
    return user_settings.keyboard_layout;
}

bool set_keyboard_layout(uint8_t layout) {
    if (layout > kKeyboardLayoutUs) return false;
    user_settings.keyboard_layout = layout;
    return save_user_settings();
}

void handle_settings_touch(int x, int y) {
    if (settings_keyboard_page) {
        if (!handle_bluetooth_touch(x, y)) {
            settings_keyboard_page = false;
            render();
        }
        return;
    }
    if (settings_rtc_page) {
        if (y >= 160 && y < 205) {
            if (x < 160) {
                settings_rtc_page = false;
                record_input("tap.settings.rtc_cancel");
            } else {
                apply_rtc_time();
                settings_rtc_page = false;
                record_input("tap.settings.rtc_save");
            }
            render();
            return;
        }

        int col = -1;
        if (x >= 12 && x < 70) col = 0;
        else if (x >= 71 && x < 129) col = 1;
        else if (x >= 130 && x < 188) col = 2;
        else if (x >= 189 && x < 247) col = 3;
        else if (x >= 248 && x < 308) col = 4;

        if (col >= 0) {
            const bool up = (y >= 56 && y < 94);
            const bool down = (y >= 120 && y < 158);
            if (up || down) {
                if (col == 0) {
                    if (up && rtc_edit.year < 2099) ++rtc_edit.year;
                    else if (down && rtc_edit.year > 2020) --rtc_edit.year;
                } else if (col == 1) {
                    if (up) rtc_edit.month = (rtc_edit.month % 12) + 1;
                    else rtc_edit.month = (rtc_edit.month == 1) ? 12 : (rtc_edit.month - 1);
                } else if (col == 2) {
                    int max_d = days_in_month(rtc_edit.year, rtc_edit.month - 1);
                    if (up) rtc_edit.day = (rtc_edit.day % max_d) + 1;
                    else rtc_edit.day = (rtc_edit.day == 1) ? max_d : (rtc_edit.day - 1);
                } else if (col == 3) {
                    if (up) rtc_edit.hour = (rtc_edit.hour + 1) % 24;
                    else rtc_edit.hour = (rtc_edit.hour == 0) ? 23 : (rtc_edit.hour - 1);
                } else if (col == 4) {
                    if (up) rtc_edit.min = (rtc_edit.min + 1) % 60;
                    else rtc_edit.min = (rtc_edit.min == 0) ? 59 : (rtc_edit.min - 1);
                }
                const int max_d = days_in_month(rtc_edit.year, rtc_edit.month - 1);
                if (rtc_edit.day > max_d) rtc_edit.day = max_d;

                record_input(up ? "tap.settings.rtc_up" : "tap.settings.rtc_down");
                render();
            }
        }
        return;
    }

    if (settings_wireless_page) {
        handle_wireless_touch(x, y);
        return;
    }

    if (settings_system_page) {
        if (y >= 158 && y < 200 && x < 160) {
            begin_calibration();
            record_input("tap.settings.calibrate");
        } else if (y >= 158 && y < 200 && x >= 160) {
            settings_system_page = false;
            record_input("tap.settings.back");
        }
        render();
        return;
    }

    // Each row owns the gap above it; the last row runs to the card's edge.
    const int row = y < kRowTop - 2 || y >= 205 ? -1 : std::min((y - (kRowTop - 2)) / kRowPitch, 5);
    if (row == 0) {
        user_settings.brightness = (user_settings.brightness + 1) % 3;
        apply_brightness();
        record_input("tap.settings.brightness");
    } else if (row == 1) {
        user_settings.theme = (user_settings.theme + 1) % 3;
        apply_theme(user_settings.theme);
        record_input("tap.settings.theme");
    } else if (row == 2) {
        user_settings.animation = (user_settings.animation + 1) % 3;
        record_input("tap.settings.motion");
    } else if (row == 3) {
        init_rtc_edit();
        settings_rtc_page = true;
        record_input("tap.settings.rtc");
        render();
        return;
    } else if (row == 4) {
        wireless_pending = -1;
        settings_wireless_page = true;
        record_input("tap.settings.wireless");
        render();
        return;
    } else if (row == 5) {
        settings_system_page = true;
        record_input("tap.settings.system");
        render();
        return;
    } else {
        return;
    }
    save_user_settings();
    render();
}

void settings_view_reset() {
    settings_rtc_page = false;
    settings_system_page = false;
    settings_wireless_page = false;
    settings_keyboard_page = false;
    wireless_pending = -1;
}

}  // namespace cyd::desktop::screen

using namespace cyd::desktop::screen;

extern "C" void cyd_desktop_settings_get(int *brightness, int *theme, int *animation) {
    if (brightness != nullptr) *brightness = user_settings.brightness;
    if (theme != nullptr) *theme = user_settings.theme;
    if (animation != nullptr) *animation = user_settings.animation;
}

extern "C" bool cyd_desktop_settings_set(int brightness, int theme, int animation) {
    if ((brightness < -1 || brightness > 2) || (theme < -1 || theme > 2) ||
        (animation < -1 || animation > 2)) return false;
    if (brightness >= 0) user_settings.brightness = brightness;
    if (theme >= 0) user_settings.theme = theme;
    if (animation >= 0) user_settings.animation = animation;
    apply_theme(user_settings.theme);
    apply_brightness();
    const bool saved = save_user_settings();
    redraw_requested = true;
    return saved;
}

extern "C" int cyd_desktop_keyboard_layout(void) {
    return keyboard_layout();
}

extern "C" bool cyd_desktop_keyboard_layout_set(int layout) {
    return layout >= 0 && layout <= kKeyboardLayoutUs && set_keyboard_layout(static_cast<uint8_t>(layout));
}

extern "C" void cyd_desktop_settings_reset_page(void) {
    settings_view_reset();
}

