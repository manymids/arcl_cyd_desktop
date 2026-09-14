#pragma once

// The views the compositor (screen.cpp) switches between. Each lives in its
// own screen_<view>.cpp and owns its state; the compositor only calls these.
//
// render_*    paints the view (called with the display mutex held).
// handle_*    reacts to a tap on the view; calls render() when it changes.
// *_touch_down draws immediate pressed feedback before the tap is handled.
// *_reset     returns the view to its first page, as the Home button does.

#include "key_input.h"
#include "shell_state.h"

namespace cyd::desktop::screen {

// Compositor
extern volatile bool redraw_requested;
extern volatile uint32_t render_count;
void render();

// Home and the Start menu (screen_home.cpp)
void render_home(const cyd::desktop::Snapshot &snapshot);
void home_touch_down(int x, int y, const cyd::desktop::Snapshot &snapshot);
void handle_home_touch(int x, int y, const cyd::desktop::Snapshot &snapshot);
void home_view_reset();

// Clock and Calendar (screen_clock.cpp)
void render_clock();
void render_calendar();
// The clock redraws only the hands and digits that changed; this forces the
// next render_clock() to paint the whole view.
void clock_view_invalidate();

// Installed MicroPython apps (screen_scripts.cpp)
void render_scripts(const cyd::desktop::Snapshot &snapshot);
void handle_scripts_touch(int x, int y, const cyd::desktop::Snapshot &snapshot);
void scripts_view_reset();

// Settings, Date & Time, System (screen_settings.cpp)
void load_user_settings();
void init_backlight_pwm();
void render_settings();
void handle_settings_touch(int x, int y);
void settings_view_reset();
// Called every UI loop pass while Settings is shown: repaints the Wireless and
// Keyboard pages when the Bluetooth state changes.
void settings_poll();
// Key layout of a Bluetooth keyboard, stored with the other user settings.
constexpr uint8_t kKeyboardLayoutJis = 0;
constexpr uint8_t kKeyboardLayoutUs = 1;
uint8_t keyboard_layout();
bool set_keyboard_layout(uint8_t layout);

// Settings > Wireless > Keyboard (screen_bluetooth.cpp)
void bluetooth_page_enter();
void bluetooth_page_prepare();
bool bluetooth_page_changed();
void paint_bluetooth_page();
// Returns false when the user leaves the page.
bool handle_bluetooth_touch(int x, int y);

// Text editor (screen_editor.cpp)
void render_editor();
void handle_editor_touch(int x, int y);
// An untitled empty buffer.
void editor_new();
// A hardware keyboard key. Returns true when the editor needs repainting.
bool editor_key(const keys::Key &key);

// Hardware keyboard routing (screen_keys.cpp): called every UI loop pass.
void poll_keyboard();

// The foreground MicroPython app: window, game and console modes (screen_app.cpp)
void render_app(const char *title);
void app_touch_down(int x, int y);
void handle_app_touch(int x, int y);
bool app_game_mode();
// Like clock_view_invalidate(): the next render_app() paints the whole window.
void app_view_invalidate();
void app_view_forget_game();

}  // namespace cyd::desktop::screen
