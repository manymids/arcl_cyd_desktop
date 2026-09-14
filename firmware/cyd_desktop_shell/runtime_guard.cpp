#include "shell_state.h"
#include "native_app.h"

#include <cstdio>
#include <cstring>

extern "C" {
#include "esp_err.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

extern "C" void cyd_desktop_ui_refresh(void);
extern "C" void cyd_desktop_app_log(const char *value);

namespace {

// A foreground app must call cyd.update() at least this often. The Task
// Watchdog resets the board when it does not: that is the only recovery
// path from a loop that never yields, since the runtime and heap limits
// below are themselves only checked inside cyd.update().
constexpr uint32_t kWatchdogMs = 15000;
constexpr uint32_t kRuntimeLimitMs = 300000;
// The heap guard stops an app that is itself driving the ESP-IDF heap toward
// exhaustion. It must not stop an app because the heap is merely low.
//
// MicroPython's split heap grows into the IDF heap on demand and never gives
// the area back, and the Wi-Fi driver keeps its buffers after an app exits.
// Measured on the board: one Wi-Fi app (-49 kB) plus one GC heap doubling from
// the MJP player (-55 kB) leaves ~14.8 kB free for the rest of the boot, and
// the system kept serving JSONL and completing HTTP fetches with the minimum
// at 10,104 B. A fixed 20,000 B floor read that steady state as exhaustion and
// sent every later app Home on its first cyd.update() - silently, since the
// only log line was "Stopped".
//
// So: stop only when free heap is under an emergency floor that sits below the
// observed healthy minimum AND this app has pulled it down since it started.
constexpr uint32_t kHeapEmergencyFloor = 8192;
constexpr uint32_t kHeapDropTolerance = 4096;
uint32_t heap_at_start = 0;
// Set by cyd.no_time_limit() for apps meant to stay open, such as the console.
bool runtime_unlimited = false;
bool watchdog_ready = false;
bool watchdog_attached = false;
TickType_t runtime_started = 0;
esp_err_t last_watchdog_error = ESP_OK;
portMUX_TYPE control_lock = portMUX_INITIALIZER_UNLOCKED;
bool runtime_paused = false;
uint32_t runtime_step_tokens = 0;
uint32_t runtime_frame = 0;

bool runtime_view_active() {
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    return snapshot.view == cyd::desktop::View::Scripts ||
        (snapshot.view == cyd::desktop::View::Native &&
         cyd::desktop::native::has_flag(snapshot.foreground_app, cyd::desktop::native::kAppScriptHosted));
}

void stop_to_home() {
    cyd::desktop::shell_state().dispatch(cyd::desktop::Action::GoHome);
    cyd_desktop_ui_refresh();
}

// True when the foreground app must be stopped. Always says why in the app log:
// a bare "Stopped" is what made the heap guard so hard to diagnose.
bool runtime_exhausted() {
    const uint32_t elapsed = static_cast<uint32_t>(xTaskGetTickCount() - runtime_started) * portTICK_PERIOD_MS;
    if (!runtime_unlimited && elapsed >= kRuntimeLimitMs) {
        cyd_desktop_app_log("Stopped: 5 minute runtime limit");
        return true;
    }
    const uint32_t free_heap = esp_get_free_heap_size();
    if (free_heap < kHeapEmergencyFloor && free_heap + kHeapDropTolerance < heap_at_start) {
        char message[48];
        std::snprintf(message, sizeof(message), "Stopped: heap %luB (start %luB)",
                      static_cast<unsigned long>(free_heap), static_cast<unsigned long>(heap_at_start));
        cyd_desktop_app_log(message);
        return true;
    }
    return false;
}

}  // namespace

extern "C" void cyd_desktop_runtime_watchdog_initialize(void) {
    if (watchdog_ready) return;
    const esp_task_wdt_config_t config = {
        .timeout_ms = kWatchdogMs,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_err_t result = esp_task_wdt_init(&config);
    if (result == ESP_ERR_INVALID_STATE) result = esp_task_wdt_reconfigure(&config);
    last_watchdog_error = result;
    watchdog_ready = result == ESP_OK;
}

extern "C" bool cyd_desktop_runtime_begin(void) {
    cyd_desktop_runtime_watchdog_initialize();
    if (!watchdog_ready || watchdog_attached) return false;
    last_watchdog_error = esp_task_wdt_add(nullptr);
    if (last_watchdog_error != ESP_OK) return false;
    watchdog_attached = true;
    runtime_started = xTaskGetTickCount();
    heap_at_start = esp_get_free_heap_size();
    runtime_unlimited = false;
    taskENTER_CRITICAL(&control_lock);
    runtime_paused = false;
    runtime_step_tokens = 0;
    runtime_frame = 0;
    taskEXIT_CRITICAL(&control_lock);
    esp_task_wdt_reset();
    return true;
}

extern "C" bool cyd_desktop_runtime_keepalive(void) {
    if (!watchdog_attached) return false;
    if (runtime_exhausted()) {
        stop_to_home();
        return false;
    }
    while (true) {
        bool advance = false;
        taskENTER_CRITICAL(&control_lock);
        if (!runtime_paused) {
            advance = true;
        } else if (runtime_step_tokens > 0) {
            --runtime_step_tokens;
            advance = true;
        }
        if (advance) ++runtime_frame;
        taskEXIT_CRITICAL(&control_lock);
        if (advance) break;
        if (!runtime_view_active()) return false;
        last_watchdog_error = esp_task_wdt_reset();
        if (last_watchdog_error != ESP_OK) return false;
        vTaskDelay(pdMS_TO_TICKS(20));
        // Time spent under external cooperative pause is not application run time.
        runtime_started += pdMS_TO_TICKS(20);
    }
    last_watchdog_error = esp_task_wdt_reset();
    return last_watchdog_error == ESP_OK;
}

extern "C" bool cyd_desktop_runtime_ping(void) {
    if (!watchdog_attached) return true;  // nothing registered, nothing to feed
    if (runtime_exhausted()) {
        stop_to_home();
        return false;
    }
    last_watchdog_error = esp_task_wdt_reset();
    return last_watchdog_error == ESP_OK;
}

extern "C" void cyd_desktop_runtime_end(void) {
    if (!watchdog_attached) return;
    last_watchdog_error = esp_task_wdt_delete(nullptr);
    watchdog_attached = false;
    taskENTER_CRITICAL(&control_lock);
    runtime_paused = false;
    runtime_step_tokens = 0;
    taskEXIT_CRITICAL(&control_lock);
}

extern "C" void cyd_desktop_runtime_set_unlimited(void) {
    if (watchdog_attached) runtime_unlimited = true;
}

extern "C" void cyd_desktop_runtime_fail(void) {
    stop_to_home();
}

extern "C" bool cyd_desktop_runtime_watchdog_ready(void) { return watchdog_ready; }
extern "C" bool cyd_desktop_runtime_watchdog_attached(void) { return watchdog_attached; }
extern "C" int cyd_desktop_runtime_watchdog_error(void) { return last_watchdog_error; }

extern "C" bool cyd_desktop_runtime_pause(void) {
    if (!watchdog_attached) return false;
    taskENTER_CRITICAL(&control_lock);
    runtime_paused = true;
    runtime_step_tokens = 0;
    taskEXIT_CRITICAL(&control_lock);
    return true;
}

extern "C" bool cyd_desktop_runtime_resume(void) {
    if (!watchdog_attached) return false;
    taskENTER_CRITICAL(&control_lock);
    runtime_paused = false;
    runtime_step_tokens = 0;
    taskEXIT_CRITICAL(&control_lock);
    return true;
}

extern "C" bool cyd_desktop_runtime_step(uint32_t frames) {
    if (!watchdog_attached || frames == 0 || frames > 3600) return false;
    taskENTER_CRITICAL(&control_lock);
    runtime_paused = true;
    const uint32_t room = 3600 - runtime_step_tokens;
    runtime_step_tokens += frames < room ? frames : room;
    taskEXIT_CRITICAL(&control_lock);
    return true;
}

extern "C" void cyd_desktop_runtime_control_state(bool *attached, bool *paused,
    uint32_t *frame, uint32_t *pending_steps) {
    if (attached != nullptr) *attached = watchdog_attached;
    taskENTER_CRITICAL(&control_lock);
    if (paused != nullptr) *paused = runtime_paused;
    if (frame != nullptr) *frame = runtime_frame;
    if (pending_steps != nullptr) *pending_steps = runtime_step_tokens;
    taskEXIT_CRITICAL(&control_lock);
}
