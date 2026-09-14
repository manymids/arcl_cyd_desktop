// Settings > Wireless > Keyboard: pair, forget and choose the key layout of the
// Bluetooth keyboard. Shown only in Bluetooth radio mode.

#include "bt_keyboard.h"
#include "screen/views.h"
#include "screen/ui_registry.h"
#include "screen/widgets.h"

#include <algorithm>
#include <cstdio>

namespace cyd::desktop::screen {

namespace {

// Taken once per render: the painter runs once per transition band, and every
// band must see the same state.
cyd_bt_snapshot_t page{};
uint32_t painted_generation = 0;
bool list_shown = false;       // SCAN was tapped: show results instead of the summary
bool forget_pending = false;

constexpr int kButtonY = 162;
constexpr int kButtonHeight = 31;
constexpr int kButtonWidth = 68;
constexpr int kButtonX[4] = {18, 90, 162, 234};
constexpr int kListTop = 50;
constexpr int kListPitch = 22;
constexpr int kListRows = 4;
constexpr int kScanSeconds = 10;

const char *display_name(const cyd_bt_found_t &device) {
    return device.name[0] != '\0' ? device.name : device.address;
}

void paint_pairing_code() {
    card(18, 50, 284, 104, 8, kCardAlt);
    char line_text[64];
    std::snprintf(line_text, sizeof(line_text), "TYPE ON %s:", page.target_name);
    centered_text_fit(line_text, 160, 58, 270, kBlack);
    centered_text(page.code, 160, 80, kWindowBlue, 4);
    centered_text("THEN PRESS ENTER", 160, 122, kBlack);
    centered_text(page.pairing == CYD_BT_PAIRING_PIN ? "PIN CODE" : "PASSKEY", 160, 138, kDarkGray);
}

void paint_message(const char *first, uint16_t first_color, const char *second, const char *third) {
    card(18, 50, 284, 104, 8, kCardAlt);
    centered_text_fit(first, 160, 72, 270, first_color);
    if (second != nullptr) centered_text_fit(second, 160, 96, 270, kDarkGray);
    if (third != nullptr) centered_text_fit(third, 160, 112, 270, kDarkGray);
}

void paint_list() {
    const int rows = std::min(page.found_count, kListRows);
    for (int index = 0; index < rows; ++index) {
        const int y = kListTop + index * kListPitch;
        const auto &device = page.found[index];
        card(18, y, 284, kListPitch - 2, 6, kCardAlt);
        text(display_name(device), 28, y + 6, kBlack);
        text(device.keyboard ? "PAIR >" : "OTHER", 256, y + 6, device.keyboard ? kWindowBlue : kDarkGray);
        char id[32];
        std::snprintf(id, sizeof(id), "settings.keyboard.found%d", index);
        ui_node(id, "button", 18, y, 284, kListPitch - 2, display_name(device));
    }
    const char *status = page.discovering ? "SCANNING... PUT THE KEYBOARD IN PAIRING MODE"
                         : page.found_count == 0 ? "NO KEYBOARDS FOUND. TAP SCAN TO RETRY"
                         : "TAP A KEYBOARD TO PAIR";
    if (rows == 0) {
        paint_message(page.discovering ? "SCANNING..." : "NO KEYBOARDS FOUND", kBlack,
                      "PUT THE KEYBOARD IN PAIRING MODE", page.discovering ? nullptr : "THEN TAP SCAN AGAIN");
    } else {
        centered_text_fit(status, 160, 142, 284, kDarkGray);
    }
}

void paint_summary() {
    card(18, 50, 284, 104, 8, kCardAlt);
    char line_text[64];
    std::snprintf(line_text, sizeof(line_text), "KEYBOARD  %s", page.bonded ? page.bonded_name : "NONE");
    text(line_text, 31, 60, kBlack);
    std::snprintf(line_text, sizeof(line_text), "STATUS    %s",
                  page.connected ? "CONNECTED" : (page.bonded ? "WAITING" : "-"));
    text(line_text, 31, 76, page.connected ? kGreen : kBlack);
    std::snprintf(line_text, sizeof(line_text), "LAYOUT    %s", keyboard_layout() == kKeyboardLayoutUs ? "US" : "JIS");
    text(line_text, 31, 92, kBlack);
    if (page.connected) {
        text("TYPE IN THE EDITOR OR THE CONSOLE.", 31, 116, kDarkGray);
    } else if (page.bonded) {
        text("PRESS A KEY TO WAKE THE KEYBOARD;", 31, 116, kDarkGray);
        text("IT RECONNECTS BY ITSELF.", 31, 132, kDarkGray);
    } else {
        text("PUT A KEYBOARD IN PAIRING MODE,", 31, 116, kDarkGray);
        text("THEN TAP SCAN.", 31, 132, kDarkGray);
    }
}

}  // namespace

void bluetooth_page_enter() {
    list_shown = false;
    forget_pending = false;
    cyd_desktop_bt_pairing_dismiss();
}

void bluetooth_page_prepare() {
    painted_generation = cyd_desktop_bt_generation();
    cyd_desktop_bt_snapshot(&page);
}

bool bluetooth_page_changed() {
    return cyd_desktop_bt_generation() != painted_generation;
}

void paint_bluetooth_page() {
    const char *subtitle = page.connected ? "CONNECTED" : (page.bonded ? "NOT CONNECTED" : "NO KEYBOARD PAIRED");
    native_page_header(UiIcon::Settings, "Keyboard", subtitle);

    if (forget_pending) {
        char line_text[64];
        std::snprintf(line_text, sizeof(line_text), "FORGET %s?", page.bonded_name);
        paint_message(line_text, kBlack, "YOU WILL NEED TO PAIR IT AGAIN.", nullptr);
        native_button(18, kButtonY, 135, kButtonHeight, "CANCEL");
        native_button(167, kButtonY, 135, kButtonHeight, "FORGET", true);
        ui_node("settings.keyboard.forget_cancel", "button", 18, kButtonY, 135, kButtonHeight, "Cancel");
        ui_node("settings.keyboard.forget_confirm", "button", 167, kButtonY, 135, kButtonHeight, "Forget");
        taskbar();
        return;
    }

    char line_text[64];
    switch (page.pairing) {
        case CYD_BT_PAIRING_PIN:
        case CYD_BT_PAIRING_PASSKEY:
            paint_pairing_code();
            break;
        case CYD_BT_PAIRING_CONNECTING:
            std::snprintf(line_text, sizeof(line_text), "CONNECTING TO %s...", page.target_name);
            paint_message(line_text, kBlack, "KEEP THE KEYBOARD IN PAIRING MODE", nullptr);
            break;
        case CYD_BT_PAIRING_DONE:
            std::snprintf(line_text, sizeof(line_text), "PAIRED: %s", page.connected ? page.connected_name : page.target_name);
            paint_message(line_text, kGreen, "THE KEYBOARD IS READY.", nullptr);
            break;
        case CYD_BT_PAIRING_FAILED:
            paint_message("PAIRING FAILED", kRed, "PUT THE KEYBOARD IN PAIRING MODE", "AND TAP SCAN AGAIN");
            break;
        default:
            if (list_shown) paint_list();
            else paint_summary();
            break;
    }

    static const char *const ids[] = {"settings.keyboard.back", "settings.keyboard.scan",
                                      "settings.keyboard.forget", "settings.keyboard.layout"};
    const char *labels[4] = {"< BACK", page.discovering ? "SCANNING" : "SCAN", "FORGET",
                             keyboard_layout() == kKeyboardLayoutUs ? "KEYS US" : "KEYS JIS"};
    for (int index = 0; index < 4; ++index) {
        native_button(kButtonX[index], kButtonY, kButtonWidth, kButtonHeight, labels[index]);
        ui_node(ids[index], "button", kButtonX[index], kButtonY, kButtonWidth, kButtonHeight, labels[index]);
    }
    taskbar();
}

// Returns false when the user leaves the page.
bool handle_bluetooth_touch(int x, int y) {
    if (forget_pending) {
        if (y >= kButtonY - 4 && y < 200) {
            if (x >= 160) {
                cyd_desktop_bt_forget();
                list_shown = false;
                record_input("tap.settings.keyboard_forget");
            } else {
                record_input("tap.settings.keyboard_forget_cancel");
            }
            forget_pending = false;
            render();
        }
        return true;
    }
    if (y >= kButtonY - 4 && y < 200) {
        const int column = x < 86 ? 0 : (x < 158 ? 1 : (x < 230 ? 2 : 3));
        if (column == 0) {
            record_input("tap.settings.back");
            cyd_desktop_bt_pairing_dismiss();
            return false;
        }
        if (column == 1) {
            cyd_desktop_bt_pairing_dismiss();
            list_shown = cyd_desktop_bt_scan(kScanSeconds) || list_shown;
            record_input("tap.settings.keyboard_scan");
        } else if (column == 2) {
            forget_pending = page.bonded;
            record_input("tap.settings.keyboard_forget_ask");
        } else {
            set_keyboard_layout(keyboard_layout() == kKeyboardLayoutUs ? kKeyboardLayoutJis : kKeyboardLayoutUs);
            record_input("tap.settings.keyboard_layout");
        }
        render();
        return true;
    }
    const bool idle = page.pairing == CYD_BT_PAIRING_IDLE;
    if (idle && list_shown && y >= kListTop && y < kListTop + kListRows * kListPitch) {
        const int index = (y - kListTop) / kListPitch;
        if (index < page.found_count && cyd_desktop_bt_connect(index)) {
            record_input("tap.settings.keyboard_pair");
            render();
        }
        return true;
    }
    if (!idle && page.pairing != CYD_BT_PAIRING_PIN && page.pairing != CYD_BT_PAIRING_PASSKEY) {
        // Tapping a finished result returns to the list or summary.
        cyd_desktop_bt_pairing_dismiss();
        render();
    }
    return true;
}

}  // namespace cyd::desktop::screen
