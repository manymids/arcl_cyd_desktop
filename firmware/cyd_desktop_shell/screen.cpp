// The compositor: owns the UI task, decides which view paints each frame,
// routes taps to the view under them, and drives native apps' frame loop.
// The views themselves live in screen_<view>.cpp (see include/screen/views.h),
// the drawing primitives in screen_draw.cpp.

#include "screen/views.h"
#include "screen/jpeg.h"
#include "screen/touch.h"
#include "screen/widgets.h"
#include "native_app.h"
#include "shell_state.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/task.h"
}

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace cyd::desktop::screen {

volatile bool redraw_requested = false;
volatile uint32_t render_count = 0;

namespace {

constexpr TickType_t kHomeLongPressTicks = pdMS_TO_TICKS(900);
bool ui_started = false;
volatile bool render_in_progress = false;
volatile uint32_t ui_generation = 0;
TickType_t native_last_frame_tick = 0;

// A view that paints the whole screen covers whatever the retained app and
// clock views last drew, so their next frame must be a full one.
void forget_retained_frames() {
    app_view_invalidate();
    clock_view_invalidate();
}

// Adapts ui_node (which has default arguments) to NativeApp::describe_ui.
void emit_native_ui_node(const char *id, const char *role, int x, int y, int width, int height,
                         const char *label) {
    ui_node(id, role, x, y, width, height, label);
}

void render_native(bool include_taskbar) {
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    if (!cyd::desktop::native::active() || cyd::desktop::native::current() == nullptr ||
        std::strcmp(cyd::desktop::native::current()->id, snapshot.foreground_app) != 0) {
        if (!cyd::desktop::native::launch(snapshot.foreground_app)) return;
    }
    const TickType_t started = xTaskGetTickCount();
    clear_clip();
    for (int y = 0; y < cyd::desktop::native::kContentHeight; y += kTransitionTileRows) {
        const int height = std::min(kTransitionTileRows, cyd::desktop::native::kContentHeight - y);
        begin_transition_tile(y, height);
        const cyd::desktop::native::TileSurface surface{
            line_buffer, kWidth, height, kWidth, y,
        };
        cyd::desktop::native::render_tile(surface);
        flush_transition_tile();
    }
    if (include_taskbar) {
        for (int y = kTaskbarY; y < kHeight; y += kTransitionTileRows) {
            begin_transition_tile(y, std::min(kTransitionTileRows, kHeight - y));
            taskbar();
            flush_transition_tile();
        }
        const auto *app = cyd::desktop::native::current();
        ui_node("native.window", "window", 0, 0, kWidth, kTaskbarY,
                app == nullptr ? "Native app" : app->title);
        if (app != nullptr && app->describe_ui != nullptr) app->describe_ui(emit_native_ui_node);
    }
    const uint32_t elapsed = static_cast<uint32_t>(xTaskGetTickCount() - started) * portTICK_PERIOD_MS;
    cyd::desktop::native::note_frame(elapsed);
}

}  // namespace

void render() {
    if (display_mutex != nullptr) xSemaphoreTake(display_mutex, portMAX_DELAY);
    render_in_progress = true;
    ++render_count;
    ui_nodes_begin();
    if (calibration_active) {
        forget_retained_frames();
        render_calibration();
        ui_node("touch.target", "button", kCalibrationTargets[calibration_step][0] - 14,
                kCalibrationTargets[calibration_step][1] - 14, 28, 28, "Calibration target");
        ui_nodes_end();
        ++ui_generation;
        render_in_progress = false;
        if (display_mutex != nullptr) xSemaphoreGive(display_mutex);
        return;
    }
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    // Video keeps the JPEG buffers between frames; any other view frees them.
    if (!(snapshot.view == cyd::desktop::View::Native &&
          cyd::desktop::native::has_flag(snapshot.foreground_app, cyd::desktop::native::kAppVideoSurface)))
        jpeg_release_buffers();
    if (snapshot.view != cyd::desktop::View::Native && cyd::desktop::native::active())
        cyd::desktop::native::leave();
    if (snapshot.view != cyd::desktop::View::Scripts) app_view_forget_game();
    if (snapshot.view == cyd::desktop::View::Home) {
        forget_retained_frames();
        render_home(snapshot);
    } else if (snapshot.view == cyd::desktop::View::Clock) {
        app_view_invalidate();
        render_clock();
    } else if (snapshot.view == cyd::desktop::View::Calendar) {
        forget_retained_frames();
        render_calendar();
    } else if (snapshot.view == cyd::desktop::View::Scripts &&
               std::strcmp(snapshot.foreground_app, "scripts") == 0) {
        forget_retained_frames();
        render_scripts(snapshot);
    } else if (snapshot.view == cyd::desktop::View::Settings) {
        forget_retained_frames();
        render_settings();
    } else if (snapshot.view == cyd::desktop::View::Native) {
        forget_retained_frames();
        render_native(true);
        native_last_frame_tick = xTaskGetTickCount();
    } else if (snapshot.view == cyd::desktop::View::Editor) {
        forget_retained_frames();
        render_editor();
    } else {
        clock_view_invalidate();
        render_app(snapshot.foreground_app);
    }
    register_taskbar_nodes();
    ui_nodes_end();
    ++ui_generation;
    render_in_progress = false;
    if (display_mutex != nullptr) xSemaphoreGive(display_mutex);
}

namespace {

void touch_down_feedback(int x, int y) {
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    if (snapshot.view == cyd::desktop::View::Native &&
        cyd::desktop::native::has_flag(snapshot.foreground_app, cyd::desktop::native::kAppVideoSurface)) return;
    if (y >= kTaskbarY) {
        if (x < 54 || (x >= 130 && x < 168)) {
            fill_round_rect(134, 214, 30, 21, 7, kWindowBlue);
            draw_icon(UiIcon::Start, 142, 219, kWhite);
        } else if (snapshot.view != cyd::desktop::View::Home && x >= 174 && x < 244) {
            fill_round_rect(176, 214, 68, 21, 7, 0x294a);
            draw_icon(UiIcon::Home, 180, 217, kAccent);
            text("BACK", 202, 222, kWhite);
        }
        return;
    }
    if (snapshot.view == cyd::desktop::View::Scripts &&
        std::strcmp(snapshot.foreground_app, "scripts") != 0) {
        app_touch_down(x, y);
        return;
    }
    if (snapshot.view != cyd::desktop::View::Home) return;
    home_touch_down(x, y, snapshot);
}

void handle_touch(int x, int y) {
    auto &shell = cyd::desktop::shell_state();
    const auto snapshot = shell.snapshot();
    const bool taskbar_start = x < 54 || (x >= 130 && x < 168);
    const bool taskbar_home_rect = x >= 174 && x < 244;
    if (y >= kTaskbarY) {
        if (taskbar_start) {
            if (snapshot.view == cyd::desktop::View::Home) {
                shell.dispatch(cyd::desktop::Action::OpenStart);
                record_input("tap.start");
            }
            render();
            return;
        } else if (taskbar_home_rect) {
            home_view_reset();
            if (snapshot.view != cyd::desktop::View::Home) {
                settings_view_reset();
                shell.dispatch(cyd::desktop::Action::GoHome);
                record_input("tap.home");
            } else {
                record_input("tap.home");
            }
            render();
            return;
        }
    }
    if (snapshot.view == cyd::desktop::View::Editor) {
        handle_editor_touch(x, y);
        return;
    }
    if (snapshot.view == cyd::desktop::View::Scripts && std::strcmp(snapshot.foreground_app, "scripts") != 0) {
        handle_app_touch(x, y);
        return;
    }
    if (snapshot.view == cyd::desktop::View::Native) {
        cyd::desktop::native::input({cyd::desktop::native::InputType::Tap,
            static_cast<int16_t>(x), static_cast<int16_t>(y)});
        record_input(x >= 220 ? "tap.native.fire" : "tap.native.control");
        return;
    }
    if (snapshot.view == cyd::desktop::View::Scripts) {
        handle_scripts_touch(x, y, snapshot);
        return;
    }
    if (snapshot.view == cyd::desktop::View::Settings) {
        handle_settings_touch(x, y);
        return;
    }
    if (snapshot.view != cyd::desktop::View::Home) return;
    handle_home_touch(x, y, snapshot);
}

void ui_task(void *) {
    if (!init_display()) vTaskDelete(nullptr);
    if (display_mutex == nullptr) display_mutex = xSemaphoreCreateMutex();
    load_user_settings();
    init_backlight_pwm();
    init_touch();
    // Uncalibrated touch input is ignored, so a new board could never reach
    // Settings > CALIBRATE: show the three targets straight away instead.
    if (!calibrated) begin_calibration();
    render();
    bool was_pressed = false;
    bool long_press_handled = false;
    bool press_has_coordinate = false;
    int press_x = 0;
    int press_y = 0;
    TickType_t press_started = 0;
    std::time_t last_clock_second = -1;
    while (true) {
        int x = 0;
        int y = 0;
        const bool pressed = touch_irq_pressed();
        physical_touch_down = pressed;
        uint16_t raw_x = 0;
        uint16_t raw_y = 0;
        const bool has_raw = pressed && read_touch_raw(&raw_x, &raw_y);
        const bool has_sample = has_raw && map_touch(raw_x, raw_y, &x, &y);
        if (has_sample) {
            last_touch_x = x;
            last_touch_y = y;
        }
        SyntheticTap synthetic{};
        if (!pressed && synthetic_taps != nullptr && xQueueReceive(synthetic_taps, &synthetic, 0) == pdTRUE) {
            last_touch_x = synthetic.x;
            last_touch_y = synthetic.y;
            ++synthetic_tap_count;
            touch_down_feedback(synthetic.x, synthetic.y);
            handle_touch(synthetic.x, synthetic.y);
        }
        if (pressed && !was_pressed) {
            if (calibration_active && has_raw) {
                calibration_record_point(raw_x, raw_y);
                render();
                was_pressed = pressed;
                delay_ms(20);
                continue;
            }
            press_has_coordinate = has_sample;
            if (has_sample) {
                press_x = x;
                press_y = y;
                touch_down_feedback(x, y);
                if (cyd::desktop::shell_state().snapshot().view == cyd::desktop::View::Native &&
                    y < kTaskbarY) {
                    cyd::desktop::native::input({cyd::desktop::native::InputType::Down,
                        static_cast<int16_t>(x), static_cast<int16_t>(y)});
                }
            }
            press_started = xTaskGetTickCount();
            long_press_handled = false;
        }
        if (pressed && !press_has_coordinate && has_sample) {
            press_x = x;
            press_y = y;
            press_has_coordinate = true;
            touch_down_feedback(x, y);
        }
        if (pressed && was_pressed && has_sample) {
            if (std::abs(x - press_x) > 8 || std::abs(y - press_y) > 8) {
                press_started = xTaskGetTickCount();
                press_x = x;
                press_y = y;
            }
        }
        if (pressed && was_pressed && has_sample &&
            cyd::desktop::shell_state().snapshot().view == cyd::desktop::View::Native && y < kTaskbarY) {
            cyd::desktop::native::input({cyd::desktop::native::InputType::Move,
                static_cast<int16_t>(x), static_cast<int16_t>(y)});
        }
        // A long press anywhere is an unconditional escape hatch. In game mode
        // or native app mode (like Neon 3D), in-app touches must not trigger Home
        // so continuous holding of movement/fire controls is preserved.
        const bool in_interactive_app = (y < kTaskbarY) && (
            (app_game_mode() && cyd::desktop::shell_state().snapshot().view == cyd::desktop::View::Scripts) ||
            (cyd::desktop::shell_state().snapshot().view == cyd::desktop::View::Native)
        );
        if (pressed && !long_press_handled && !in_interactive_app &&
            xTaskGetTickCount() - press_started >= kHomeLongPressTicks) {
            cyd::desktop::shell_state().dispatch(cyd::desktop::Action::GoHome);
            record_input("long_press.home");
            long_press_handled = true;
            render();
        }
        if (!pressed && was_pressed && press_has_coordinate && !long_press_handled) {
            if (cyd::desktop::shell_state().snapshot().view == cyd::desktop::View::Native &&
                press_y < kTaskbarY) {
                cyd::desktop::native::input({cyd::desktop::native::InputType::Up,
                    static_cast<int16_t>(press_x), static_cast<int16_t>(press_y)});
            } else {
                handle_touch(press_x, press_y);
            }
        }
        was_pressed = pressed;
        if (redraw_requested) {
            render_in_progress = true;
            redraw_requested = false;
            render();
        }
        poll_keyboard();
        if (cyd::desktop::shell_state().snapshot().view == cyd::desktop::View::Settings) settings_poll();
        if (cyd::desktop::shell_state().snapshot().view == cyd::desktop::View::Clock) {
            const std::time_t now = std::time(nullptr);
            if (now != last_clock_second) {
                last_clock_second = now;
                render_clock();
            }
        }
        if (cyd::desktop::shell_state().snapshot().view == cyd::desktop::View::Native &&
            cyd::desktop::native::active() && cyd::desktop::native::current() != nullptr) {
            const TickType_t now = xTaskGetTickCount();
            const uint32_t elapsed = static_cast<uint32_t>(now - native_last_frame_tick) * portTICK_PERIOD_MS;
            if (elapsed >= cyd::desktop::native::current()->target_frame_ms) {
                native_last_frame_tick = now;
                if (cyd::desktop::native::update(elapsed)) {
                    render_in_progress = true;
                    ++render_count;
                    render_native(false);
                    render_in_progress = false;
                }
            }
        }
        delay_ms(cyd::desktop::shell_state().snapshot().view == cyd::desktop::View::Native ? 5 : 30);
    }
}

}  // namespace

}  // namespace cyd::desktop::screen

using namespace cyd::desktop::screen;

extern "C" void cyd_desktop_ui_start(void) {
    if (ui_started) return;
    ui_started = true;
    // 4096 left under 1 KB unused while an app or video was on screen:
    // render_app copies the 1.3 KB app surface onto this stack.
    xTaskCreatePinnedToCore(ui_task, "cyd-ui", 6144, nullptr, 4, nullptr, tskNO_AFFINITY);
}

extern "C" void cyd_desktop_ui_refresh(void) {
    const uint32_t previous = ui_generation;
    redraw_requested = true;
    for (int attempt = 0; attempt < 300; ++attempt) {
        if (!redraw_requested && !render_in_progress && ui_generation != previous) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C" void cyd_desktop_ui_refresh_async(void) { redraw_requested = true; }

extern "C" void cyd_desktop_render_metrics(uint32_t *renders, uint32_t *transactions,
    uint64_t *bytes, uint32_t *injected, uint32_t *fingerprint) {
    if (renders != nullptr) *renders = render_count;
    if (fingerprint != nullptr) *fingerprint = spi_fingerprint;
    if (transactions != nullptr) *transactions = spi_transactions;
    if (bytes != nullptr) *bytes = spi_bytes;
    if (injected != nullptr) *injected = synthetic_tap_count;
}

extern "C" void cyd_desktop_render_fingerprint_enable(bool enabled) {
    if (enabled && !spi_fingerprint_enabled) spi_fingerprint = 0;
    spi_fingerprint_enabled = enabled;
}

extern "C" bool cyd_desktop_render_fingerprint_enabled(void) {
    return spi_fingerprint_enabled;
}

