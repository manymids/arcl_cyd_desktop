#include "screen/widgets.h"
#include "bt_keyboard.h"
#include "shell_state.h"

namespace cyd::desktop::screen {

uint16_t kWallpaperTop = 0x08a5;
uint16_t kWallpaperBand = 0x096b;
uint16_t kWallpaperMid = 0x11cc;
uint16_t kWallpaperBottom = 0x1b15;
uint16_t kWallpaperGlowTop = 0x1a70;
uint16_t kWallpaperGlowBottom = 0x22b7;
uint16_t kWindowBlue = 0x231d;
uint16_t kAccent = 0x653f;
uint16_t kAccentSoft = 0xad9f;

void apply_theme(uint8_t theme) {
    switch (theme) {
        case 1:
            kWallpaperTop = 0x180a; kWallpaperBand = 0x2811; kWallpaperMid = 0x3819;
            kWallpaperBottom = 0x501f; kWallpaperGlowTop = 0x681f; kWallpaperGlowBottom = 0x913f;
            kWindowBlue = 0x71df; kAccent = 0xaaff; kAccentSoft = 0xd59f;
            break;
        case 2:
            kWallpaperTop = 0x0228; kWallpaperBand = 0x034d; kWallpaperMid = 0x04b2;
            kWallpaperBottom = 0x0617; kWallpaperGlowTop = 0x0798; kWallpaperGlowBottom = 0x371b;
            kWindowBlue = 0x05d6; kAccent = 0x67fb; kAccentSoft = 0xafbd;
            break;
        default:
            kWallpaperTop = 0x08a5; kWallpaperBand = 0x096b; kWallpaperMid = 0x11cc;
            kWallpaperBottom = 0x1b15; kWallpaperGlowTop = 0x1a70; kWallpaperGlowBottom = 0x22b7;
            kWindowBlue = 0x231d; kAccent = 0x653f; kAccentSoft = 0xad9f;
            break;
    }
}

void card(int x, int y, int width, int height, int radius, uint16_t fill) {
    fill_round_rect(x + 2, y + 3, width, height, radius, kShadow);
    round_rect(x, y, width, height, radius, kAccentSoft, fill);
}

void wallpaper() {
    fill_rect(0, 0, kWidth, 54, kWallpaperTop);
    fill_rect(0, 54, kWidth, 54, kWallpaperBand);
    fill_rect(0, 108, kWidth, 52, kWallpaperMid);
    fill_rect(0, 160, kWidth, kTaskbarY - 160, kWallpaperBottom);
    fill_round_rect(236, 18, 112, 48, 24, kWallpaperGlowTop);
    fill_round_rect(-30, 158, 126, 42, 21, kWallpaperGlowBottom);
}

void draw_icon(UiIcon icon, int x, int y, uint16_t color) {
    switch (icon) {
        case UiIcon::Start:
            fill_rect(x, y, 6, 6, color); fill_rect(x + 8, y, 6, 6, color);
            fill_rect(x, y + 8, 6, 6, color); fill_rect(x + 8, y + 8, 6, 6, color);
            break;
        case UiIcon::Clock:
            circle(x + 8, y + 8, 7, color, 2);
            line(x + 8, y + 8, x + 8, y + 4, color, 2);
            line(x + 8, y + 8, x + 12, y + 10, color, 2);
            break;
        case UiIcon::Calendar:
            round_rect(x + 1, y + 2, 15, 14, 3, color, kCard);
            fill_rect(x + 2, y + 5, 13, 3, color);
            fill_rect(x + 4, y, 2, 5, color); fill_rect(x + 11, y, 2, 5, color);
            fill_rect(x + 4, y + 10, 3, 2, color); fill_rect(x + 10, y + 10, 3, 2, color);
            break;
        case UiIcon::Scripts:
            round_rect(x + 2, y, 13, 16, 2, color, kCard);
            fill_rect(x + 5, y + 5, 7, 1, color);
            fill_rect(x + 5, y + 8, 7, 1, color);
            fill_rect(x + 5, y + 11, 5, 1, color);
            break;
        case UiIcon::Settings:
            circle(x + 8, y + 8, 5, color, 2);
            fill_circle(x + 8, y + 8, 2, color);
            line(x + 8, y, x + 8, y + 3, color, 2); line(x + 8, y + 13, x + 8, y + 16, color, 2);
            line(x, y + 8, x + 3, y + 8, color, 2); line(x + 13, y + 8, x + 16, y + 8, color, 2);
            break;
        case UiIcon::Home:
            line(x + 1, y + 8, x + 8, y + 1, color, 2);
            line(x + 8, y + 1, x + 15, y + 8, color, 2);
            frame(x + 3, y + 8, 11, 8, color);
            fill_rect(x + 7, y + 11, 3, 5, color);
            break;
        case UiIcon::Game:
            circle(x + 8, y + 8, 7, color, 1);
            line(x + 8, y + 1, x + 8, y + 5, color, 2);
            line(x + 8, y + 11, x + 8, y + 15, color, 2);
            line(x + 1, y + 8, x + 5, y + 8, color, 2);
            line(x + 11, y + 8, x + 15, y + 8, color, 2);
            fill_circle(x + 8, y + 8, 2, color);
            break;
        case UiIcon::Movie:
            frame(x, y + 2, 16, 12, color);
            fill_rect(x + 2, y + 4, 3, 2, color);
            fill_rect(x + 11, y + 4, 3, 2, color);
            fill_rect(x + 2, y + 10, 3, 2, color);
            fill_rect(x + 11, y + 10, 3, 2, color);
            line(x + 7, y + 5, x + 11, y + 8, color, 1);
            line(x + 11, y + 8, x + 7, y + 11, color, 1);
            break;
        case UiIcon::Editor:
            round_rect(x + 1, y + 1, 14, 15, 2, color, kCard);
            line(x + 4, y + 4, x + 12, y + 4, color, 1);
            line(x + 4, y + 7, x + 12, y + 7, color, 1);
            line(x + 4, y + 10, x + 10, y + 10, color, 1);
            line(x + 9, y + 13, x + 14, y + 8, color, 2);
            break;
    }
}

void taskbar() {
    fill_rect(0, kTaskbarY, kWidth, kHeight - kTaskbarY, kWallpaperTop);
    fill_round_rect(73, 214, 176, 25, 10, kShadow);
    round_rect(72, 212, 176, 25, 10, 0x31c7, kTaskbarBlue);
    fill_round_rect(134, 214, 30, 21, 7, 0x294a);
    draw_icon(UiIcon::Start, 142, 218, kAccent);
    fill_rect(171, 217, 1, 15, 0x4a69);
    draw_icon(UiIcon::Home, 180, 217, kWhite);
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    text(snapshot.view == cyd::desktop::View::Home ? "HOME" : "BACK", 202, 222, kWhite);
    fill_circle(92, 224, 3, kGreen);
    text("CYD", 100, 222, kAccentSoft);
    taskbar_keyboard_status();
}

constexpr int kKeyboardStatusX = 262;
constexpr int kKeyboardStatusY = 218;

void taskbar_keyboard_status() {
    const int state = cyd_desktop_bt_keyboard_state();
    if (state == 0) return;
    const uint16_t color = state == 2 ? kGreen : kYellow;
    fill_rect(kKeyboardStatusX - 4, kTaskbarY, 30, kHeight - kTaskbarY, kWallpaperTop);
    // A small keyboard: outline, two rows of keys and a space bar.
    round_rect(kKeyboardStatusX, kKeyboardStatusY, 22, 14, 3, color, 0x10a2);
    for (int column = 0; column < 4; ++column) {
        fill_rect(kKeyboardStatusX + 3 + column * 4, kKeyboardStatusY + 3, 3, 2, color);
        fill_rect(kKeyboardStatusX + 5 + column * 4 - (column == 3 ? 2 : 0), kKeyboardStatusY + 6, 3, 2, color);
    }
    fill_rect(kKeyboardStatusX + 6, kKeyboardStatusY + 10, 10, 2, color);
    if (state == 1) fill_rect(kKeyboardStatusX - 1, kKeyboardStatusY + 13, 24, 1, color);
}

void register_taskbar_nodes() {
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    ui_node("taskbar.start", "button", 134, 214, 30, 21, "Start");
    if (snapshot.view != cyd::desktop::View::Home)
        ui_node("taskbar.home", "button", 176, 214, 68, 21, "Home");
    const int keyboard = cyd_desktop_bt_keyboard_state();
    if (keyboard != 0)
        ui_node("taskbar.keyboard", "status", kKeyboardStatusX, kKeyboardStatusY, 22, 14,
                keyboard == 2 ? "Keyboard connected" : "Keyboard not connected", false);
}

void native_button(int x, int y, int width, int height, const char *label, bool danger) {
    fill_round_rect(x + 1, y + 2, width, height, 7, kShadow);
    const uint16_t fill = danger ? kRed : kWindowBlue;
    round_rect(x, y, width, height, 7, danger ? 0xfbcf : kAccent, fill);
    centered_text_fit(label, x + width / 2, y + (height - 7) / 2, width - 8, kWhite);
}

void native_page_header(UiIcon icon, const char *title, const char *subtitle) {
    wallpaper();
    card(8, 8, 304, 197, 13, kCard);
    draw_icon(icon, 22, 18, kWindowBlue);
    text(title, 48, 18, kBlack, 2);
    if (subtitle != nullptr) text(subtitle, 49, 36, kDarkGray);
}

}  // namespace cyd::desktop::screen
