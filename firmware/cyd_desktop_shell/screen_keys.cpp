// Routes hardware keyboard keys to whatever has the screen.
//
// Keys come from the Bluetooth keyboard (usages, translated here with the
// layout from Settings) or from the protocol's desktop_key (already named).
// The UI task drains both every loop pass:
//
//   Editor view                 the editor edits its text
//   a running MicroPython app   Ctrl+C raises KeyboardInterrupt; other keys
//                               queue for cyd.key()
//   anything else               the key is dropped

#include "bt_keyboard.h"
#include "key_input.h"
#include "native_app.h"
#include "screen/display.h"
#include "screen/draw.h"
#include "screen/theme.h"
#include "screen/views.h"
#include "screen/ui_registry.h"
#include "screen/widgets.h"
#include "shell_state.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "py/runtime.h"
}

#include <cstdio>
#include <cstring>

extern "C" bool cyd_desktop_runtime_watchdog_attached(void);
extern "C" bool cyd_desktop_app_stop_requested(void);

namespace cyd::desktop::screen {

namespace {

constexpr size_t kQueueLength = 32;

struct KeyQueue {
    keys::Key values[kQueueLength];
    size_t head;
    size_t count;

    bool push(const keys::Key &key) {
        if (count >= kQueueLength) return false;
        values[(head + count) % kQueueLength] = key;
        ++count;
        return true;
    }

    bool pop(keys::Key *key) {
        if (count == 0) return false;
        *key = values[head];
        head = (head + 1) % kQueueLength;
        --count;
        return true;
    }
};

portMUX_TYPE queue_lock = portMUX_INITIALIZER_UNLOCKED;
KeyQueue injected{};   // desktop_key, from the protocol task
KeyQueue app_keys{};   // for cyd.key(), read by the MicroPython task
bool caps_lock = false;
bool keyboard_was_connected = false;
int shown_keyboard_state = 0;
TickType_t toast_until = 0;
constexpr TickType_t kToastTicks = pdMS_TO_TICKS(3000);

bool next_key(keys::Key *key) {
    taskENTER_CRITICAL(&queue_lock);
    const bool from_protocol = injected.pop(key);
    taskEXIT_CRITICAL(&queue_lock);
    if (from_protocol) return true;
    cyd_bt_key_t raw;
    while (cyd_desktop_bt_key_next(&raw)) {
        *key = keys::translate(raw.usage, raw.modifiers, static_cast<keys::Layout>(keyboard_layout()), caps_lock);
        if (key->code != keys::kNone) return true;
    }
    return false;
}

bool app_running() {
    return cyd_desktop_runtime_watchdog_attached() && !cyd_desktop_app_stop_requested();
}

// Views that repaint on their own every frame, or own the whole panel, get no
// overlay: video covers the taskbar, and a game or Neon 3D would erase it at once.
bool overlay_allowed() {
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    if (snapshot.view == cyd::desktop::View::Native) {
        return !cyd::desktop::native::has_flag(snapshot.foreground_app, cyd::desktop::native::kAppVideoSurface);
    }
    return !(snapshot.view == cyd::desktop::View::Scripts && app_game_mode());
}

// Drawn straight to the panel between renders, under the display lock that
// renders and JPEG output take.
bool lock_display() {
    return display_mutex == nullptr || xSemaphoreTake(display_mutex, pdMS_TO_TICKS(200)) == pdTRUE;
}

void unlock_display() {
    if (display_mutex != nullptr) xSemaphoreGive(display_mutex);
}

void refresh_taskbar_status() {
    const int state = cyd_desktop_bt_keyboard_state();
    if (state == shown_keyboard_state) return;
    shown_keyboard_state = state;
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    if (snapshot.view == cyd::desktop::View::Native &&
        cyd::desktop::native::has_flag(snapshot.foreground_app, cyd::desktop::native::kAppVideoSurface)) return;
    if (!lock_display()) return;
    clear_clip();
    fill_rect(254, kTaskbarY, 34, kHeight - kTaskbarY, kWallpaperTop);
    taskbar_keyboard_status();
    unlock_display();
}

// A one-line banner above the taskbar for three seconds. The view repaints
// over it when it expires.
void show_toast(const char *message, uint16_t color) {
    if (!overlay_allowed() || !lock_display()) return;
    clear_clip();
    fill_round_rect(9, 189, 304, 20, 7, kShadow);
    round_rect(8, 188, 304, 20, 7, color, 0x10a2);
    centered_text_fit(message, 160, 195, 292, color);
    unlock_display();
    toast_until = xTaskGetTickCount() + kToastTicks;
    if (toast_until == 0) toast_until = 1;
}

}  // namespace

void poll_keyboard() {
    bool repaint = false;
    // The editor hides its on-screen keyboard while a hardware one is connected.
    const bool connected = cyd_desktop_bt_connected();
    const bool connection_changed = connected != keyboard_was_connected;
    if (connection_changed) {
        keyboard_was_connected = connected;
        if (cyd::desktop::shell_state().snapshot().view == cyd::desktop::View::Editor) repaint = true;
    }
    keys::Key key;
    // Bounded so a held key cannot keep the UI task here.
    for (int handled = 0; handled < 48 && next_key(&key); ++handled) {
        if (key.code == keys::kCapsLock) {
            caps_lock = !caps_lock;
            continue;
        }
        const auto view = cyd::desktop::shell_state().snapshot().view;
        if (view == cyd::desktop::View::Editor) {
            repaint = editor_key(key) || repaint;
        } else if (app_running()) {
            if (keys::is_ctrl(key, 'c')) {
                // Raised in the app at its next bytecode, even inside a busy loop.
                mp_sched_keyboard_interrupt();
                record_event("key.interrupt");
            } else {
                taskENTER_CRITICAL(&queue_lock);
                app_keys.push(key);
                taskEXIT_CRITICAL(&queue_lock);
            }
        }
    }
    if (repaint) render();
    refresh_taskbar_status();
    if (connection_changed) {
        // A sleeping keyboard wakes on a key press, and presses made before the
        // link is back are lost; say so when it goes.
        if (connected) show_toast("KEYBOARD CONNECTED", kGreen);
        else show_toast("KEYBOARD OFF: KEYS ARE LOST UNTIL IT RECONNECTS", kYellow);
    }
    if (toast_until != 0 && xTaskGetTickCount() >= toast_until) {
        toast_until = 0;
        app_view_invalidate();
        clock_view_invalidate();
        redraw_requested = true;
    }
}

}  // namespace cyd::desktop::screen

using namespace cyd::desktop::screen;
namespace keys = cyd::desktop::keys;

extern "C" bool cyd_desktop_key_inject(const char *name) {
    keys::Key key;
    if (!keys::parse(name, &key)) return false;
    taskENTER_CRITICAL(&queue_lock);
    const bool queued = injected.push(key);
    taskEXIT_CRITICAL(&queue_lock);
    return queued;
}

extern "C" bool cyd_desktop_app_key_read(char *out, size_t capacity) {
    keys::Key key;
    taskENTER_CRITICAL(&queue_lock);
    const bool found = app_keys.pop(&key);
    taskEXIT_CRITICAL(&queue_lock);
    return found && keys::name(key, out, capacity);
}

extern "C" void cyd_desktop_app_keys_clear(void) {
    taskENTER_CRITICAL(&queue_lock);
    app_keys = {};
    taskEXIT_CRITICAL(&queue_lock);
}
