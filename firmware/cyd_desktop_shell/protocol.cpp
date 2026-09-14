#include "shell_state.h"
#include "native_app.h"
#include "bt_keyboard.h"
#include "desktop_version.h"
#include "json_buffer.h"
#include "json_parse.h"

extern "C" {
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "esp_system.h"
}

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/time.h>

extern "C" void cyd_desktop_ui_refresh(void);
extern "C" bool cyd_desktop_touch_calibrated(void);
extern "C" void cyd_desktop_touch_calibration_start(void);
extern "C" bool cyd_desktop_input_read(char *event, size_t capacity, bool clear);
extern "C" bool cyd_desktop_sd_mount(void);
extern "C" void cyd_desktop_app_error_get(char *app_id, size_t app_id_capacity,
                                         char *text, size_t text_capacity);
extern "C" bool cyd_desktop_sd_mounted(void);
extern "C" int cyd_desktop_sd_last_error(void);
extern "C" int cyd_desktop_sd_boot_error(void);
extern "C" bool cyd_desktop_package_submit(int operation, const char *app_id, const char *filename,
    const char *payload, uint32_t expected_size, const char *sha256, char *error, size_t error_capacity);
extern "C" bool cyd_desktop_runtime_watchdog_ready(void);
extern "C" bool cyd_desktop_runtime_watchdog_attached(void);
extern "C" int cyd_desktop_runtime_watchdog_error(void);
extern "C" bool cyd_desktop_touch_inject(int x, int y);
extern "C" void cyd_desktop_touch_state(bool *pressed, int *x, int *y, unsigned *pending);
extern "C" unsigned cyd_desktop_ui_node_count(void);
extern "C" bool cyd_desktop_ui_node_get(unsigned index, char *id, size_t id_capacity,
    char *role, size_t role_capacity, char *label, size_t label_capacity,
    int *x, int *y, int *width, int *height, bool *enabled);
extern "C" unsigned cyd_desktop_event_count(void);
extern "C" bool cyd_desktop_event_get(unsigned index, uint32_t *tick_ms, char *value, size_t capacity);
extern "C" void cyd_desktop_render_metrics(uint32_t *renders, uint32_t *transactions,
    uint64_t *bytes, uint32_t *injected, uint32_t *fingerprint);
extern "C" void cyd_desktop_render_fingerprint_enable(bool enabled);
extern "C" int cyd_desktop_keyboard_layout(void);
extern "C" bool cyd_desktop_apps_rescan_async(void);
extern "C" bool cyd_desktop_key_inject(const char *name);
extern "C" void cyd_desktop_ui_refresh_async(void);
extern "C" bool cyd_desktop_keyboard_layout_set(int layout);
extern "C" bool cyd_desktop_render_fingerprint_enabled(void);
extern "C" void cyd_desktop_settings_get(int *brightness, int *theme, int *animation);
extern "C" bool cyd_desktop_settings_set(int brightness, int theme, int animation);
extern "C" void cyd_desktop_settings_reset_page(void);
extern "C" bool cyd_desktop_runtime_pause(void);
extern "C" bool cyd_desktop_runtime_resume(void);
extern "C" bool cyd_desktop_runtime_step(uint32_t frames);
extern "C" void cyd_desktop_runtime_control_state(bool *attached, bool *paused,
    uint32_t *frame, uint32_t *pending_steps);

namespace {

constexpr int kLineLimit = 2600;
constexpr long kUnixEpochAtYear2000 = 946684800L;
bool protocol_started = false;

const char *view_name(cyd::desktop::View view) {
    switch (view) {
        case cyd::desktop::View::Home: return "home";
        case cyd::desktop::View::Clock: return "clock";
        case cyd::desktop::View::Calendar: return "calendar";
        case cyd::desktop::View::Scripts: return "scripts";
        case cyd::desktop::View::Settings: return "settings";
        case cyd::desktop::View::Native: return "native";
        case cyd::desktop::View::Editor: return "editor";
        default: return "unknown";
    }
}

bool json_string(const char *line, const char *key, char *out, size_t capacity) {
    return cyd::desktop::json::string_value(line, key, out, capacity);
}

bool json_integer(const char *line, const char *key, long long *value) {
    return cyd::desktop::json::integer_value(line, key, value);
}

void write_line(const char *value) {
    // Every response this file builds is a JSON object, so it ends in '}'. If it
    // does not, snprintf truncated it somewhere upstream and the line is
    // unparseable. One check here covers all 26 response sites. Converting each
    // of them individually is far more churn than the failure it guards, and the
    // four variable-length list builders already report overflow precisely.
    const size_t length = std::strlen(value);
    if (length == 0 || value[length - 1] != '}') {
        static const char kTruncated[] =
            "{\"id\":\"\",\"ok\":false,\"error\":{\"code\":\"RESPONSE_TOO_LARGE\","
            "\"message\":\"response did not fit the transmit buffer\"}}";
        uart_write_bytes(UART_NUM_0, kTruncated, std::strlen(kTruncated));
        uart_write_bytes(UART_NUM_0, "\n", 1);
        return;
    }
    uart_write_bytes(UART_NUM_0, value, length);
    uart_write_bytes(UART_NUM_0, "\n", 1);
}

void json_escape(const char *source, char *destination, size_t capacity) {
    cyd::desktop::json::escape(source, destination, capacity);
}

void error(const char *id, const char *code, const char *message) {
    char response[512];
    std::snprintf(response, sizeof(response),
                  "{\"id\":\"%s\",\"ok\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
                  id, code, message);
    write_line(response);
}

void status(const char *id) {
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    char response[512];
    std::snprintf(response, sizeof(response),
                  "{\"id\":\"%s\",\"ok\":true,\"result\":{\"version\":\"" CYD_DESKTOP_VERSION "\",\"view\":\"%s\",\"start_open\":%s,\"foreground\":\"%s\",\"touch_calibrated\":%s,\"installed_apps\":%u,\"shortcuts\":%u,\"time_epoch\":%ld}}",
                  id, view_name(snapshot.view), snapshot.start_open ? "true" : "false", snapshot.foreground_app,
                  cyd_desktop_touch_calibrated() ? "true" : "false", snapshot.app_count, snapshot.shortcut_count,
                  static_cast<long>(std::time(nullptr)) + kUnixEpochAtYear2000);
    write_line(response);
}

bool launch(const char *app) {
    auto &shell = cyd::desktop::shell_state();
    if (std::strcmp(app, "clock") == 0) return shell.dispatch(cyd::desktop::Action::LaunchClock);
    if (std::strcmp(app, "calendar") == 0) return shell.dispatch(cyd::desktop::Action::LaunchCalendar);
    if (std::strcmp(app, "scripts") == 0) {
        cyd_desktop_apps_rescan_async();
        return shell.dispatch(cyd::desktop::Action::LaunchScripts);
    }
    if (std::strcmp(app, "settings") == 0) {
        cyd_desktop_settings_reset_page();
        return shell.dispatch(cyd::desktop::Action::LaunchSettings);
    }
    if (cyd::desktop::native::has_flag(app, cyd::desktop::native::kAppLaunchable))
        return shell.launch_native(app);
    if (std::strcmp(app, "editor") == 0) return shell.dispatch(cyd::desktop::Action::LaunchEditor);
    return false;
}

bool safe_component(const char *value, size_t maximum) {
    const size_t length = std::strlen(value);
    if (length == 0 || length > maximum || std::strstr(value, "..") != nullptr) return false;
    for (size_t index = 0; index < length; ++index) {
        const unsigned char character = static_cast<unsigned char>(value[index]);
        if (!std::isalnum(character) && character != '_' && character != '-' && character != '.') return false;
    }
    return true;
}

bool safe_title(const char *value) {
    const size_t length = std::strlen(value);
    if (length == 0 || length > 12) return false;
    for (size_t index = 0; index < length; ++index) {
        const unsigned char character = static_cast<unsigned char>(value[index]);
        if (!std::isalnum(character) && character != ' ' && character != '_' && character != '-' && character != '.') return false;
    }
    return true;
}

bool script_launch(const char *app_id, char *worker_error, size_t worker_error_capacity) {
    return cyd_desktop_package_submit(3, app_id, "", "", 0, "", worker_error, worker_error_capacity);
}

void shortcuts_list(const char *id, unsigned offset, unsigned limit) {
    cyd::desktop::Shortcut shortcuts[cyd::desktop::ShellState::kMaxShortcuts];
    const unsigned total = cyd::desktop::shell_state().copy_shortcuts(
        shortcuts, cyd::desktop::ShellState::kMaxShortcuts);
    if (offset > total) offset = total;
    limit = std::max(1u, std::min(limit, 6u));
    char items[640] = {};
    size_t used = 0;
    unsigned returned = 0;
    for (unsigned index = offset; index < total && returned < limit; ++index) {
        const auto &shortcut = shortcuts[index];
        if (!cyd::desktop::json::items_append(items, sizeof(items), used,
                "%s{\"shortcut_id\":\"%s\",\"app_id\":\"%s\",\"title\":\"%s\"}",
                returned == 0 ? "" : ",", shortcut.id, shortcut.app_id, shortcut.title)) {
            break;
        }
        ++returned;
    }
    const unsigned next = offset + returned;
    char response[832];
    const int written = std::snprintf(response, sizeof(response),
        "{\"id\":\"%s\",\"ok\":true,\"result\":{\"offset\":%u,\"total\":%u,\"next\":%u,"
        "\"returned\":%u,\"truncated\":%s,\"shortcuts\":[%s]}}",
        id, offset, total, next, returned, next < total ? "true" : "false", items);
    if (cyd::desktop::json::response_truncated(written, sizeof(response))) {
        error(id, "RESPONSE_TOO_LARGE", "shortcut page did not fit the response buffer");
        return;
    }
    write_line(response);
}

void ui_tree_page(const char *id, unsigned offset, unsigned limit) {
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    const unsigned total = cyd_desktop_ui_node_count();
    if (offset > total) offset = total;
    limit = std::max(1u, std::min(limit, 4u));
    char items[600] = {};
    size_t used = 0;
    unsigned returned = 0;
    for (unsigned index = offset; index < total && returned < limit; ++index) {
        char node_id[41] = {};
        char role[13] = {};
        char label[25] = {};
        char escaped_label[50] = {};
        int x = 0, y = 0, width = 0, height = 0;
        bool enabled = false;
        if (!cyd_desktop_ui_node_get(index, node_id, sizeof(node_id), role, sizeof(role),
                                     label, sizeof(label), &x, &y, &width, &height, &enabled)) break;
        json_escape(label, escaped_label, sizeof(escaped_label));
        if (!cyd::desktop::json::items_append(items, sizeof(items), used,
                "%s{\"id\":\"%s\",\"role\":\"%s\",\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"label\":\"%s\",\"enabled\":%s}",
                returned == 0 ? "" : ",", node_id, role, x, y, width, height, escaped_label,
                enabled ? "true" : "false")) {
            break;
        }
        ++returned;
    }
    const unsigned next = offset + returned;
    char response[768];
    const int written = std::snprintf(response, sizeof(response),
        "{\"id\":\"%s\",\"ok\":true,\"result\":{\"view\":\"%s\",\"offset\":%u,\"total\":%u,\"next\":%u,\"nodes\":[%s]}}",
        id, view_name(snapshot.view), offset, total, next, items);
    if (cyd::desktop::json::response_truncated(written, sizeof(response))) {
        error(id, "RESPONSE_TOO_LARGE", "ui tree page did not fit the response buffer");
        return;
    }
    write_line(response);
}

void installed_apps_page(const char *id, unsigned offset, unsigned limit) {
    cyd::desktop::InstalledApp apps[cyd::desktop::ShellState::kMaxInstalledApps];
    cyd::desktop::Shortcut shortcuts[cyd::desktop::ShellState::kMaxShortcuts];
    auto &shell = cyd::desktop::shell_state();
    const unsigned app_total = shell.copy_apps(apps, cyd::desktop::ShellState::kMaxInstalledApps);
    const uint8_t shortcut_total = shell.copy_shortcuts(shortcuts, cyd::desktop::ShellState::kMaxShortcuts);
    if (offset > app_total) offset = app_total;
    limit = std::max(1u, std::min(limit, 6u));
    char items[520] = {};
    size_t used = 0;
    unsigned returned = 0;
    for (unsigned index = offset; index < app_total && returned < limit; ++index) {
        const auto &app = apps[index];
        bool pinned = false;
        for (uint8_t shortcut = 0; shortcut < shortcut_total; ++shortcut)
            if (std::strcmp(shortcuts[shortcut].app_id, app.app_id) == 0) pinned = true;
        if (!cyd::desktop::json::items_append(items, sizeof(items), used,
                "%s{\"app_id\":\"%s\",\"title\":\"%s\",\"pinned\":%s}",
                returned == 0 ? "" : ",", app.app_id, app.title, pinned ? "true" : "false")) {
            break;
        }
        ++returned;
    }
    char response[700];
    const int written = std::snprintf(response, sizeof(response),
        "{\"id\":\"%s\",\"ok\":true,\"result\":{\"offset\":%u,\"total\":%u,\"next\":%u,\"apps\":[%s]}}",
        id, offset, app_total, offset + returned, items);
    if (cyd::desktop::json::response_truncated(written, sizeof(response))) {
        error(id, "RESPONSE_TOO_LARGE", "installed app page did not fit the response buffer");
        return;
    }
    write_line(response);
}

void event_log_page(const char *id, unsigned offset, unsigned limit) {
    const unsigned total = cyd_desktop_event_count();
    if (offset > total) offset = total;
    limit = std::max(1u, std::min(limit, 4u));
    char items[480] = {};
    size_t used = 0;
    unsigned returned = 0;
    for (unsigned index = offset; index < total && returned < limit; ++index) {
        uint32_t tick_ms = 0;
        char value[49] = {};
        char escaped[98] = {};
        if (!cyd_desktop_event_get(index, &tick_ms, value, sizeof(value))) break;
        json_escape(value, escaped, sizeof(escaped));
        if (!cyd::desktop::json::items_append(items, sizeof(items), used,
                "%s{\"tick_ms\":%lu,\"value\":\"%s\"}", returned == 0 ? "" : ",",
                static_cast<unsigned long>(tick_ms), escaped)) {
            break;
        }
        ++returned;
    }
    char response[650];
    const int written = std::snprintf(response, sizeof(response),
        "{\"id\":\"%s\",\"ok\":true,\"result\":{\"offset\":%u,\"total\":%u,\"next\":%u,\"events\":[%s]}}",
        id, offset, total, offset + returned, items);
    if (cyd::desktop::json::response_truncated(written, sizeof(response))) {
        error(id, "RESPONSE_TOO_LARGE", "event page did not fit the response buffer");
        return;
    }
    write_line(response);
}

// The last app failure with its full traceback (see app_error.cpp). The app
// log keeps only a 48-character summary.
void app_error_report(const char *id) {
    char app_id[25] = {};
    char traceback[1024] = {};
    cyd_desktop_app_error_get(app_id, sizeof(app_id), traceback, sizeof(traceback));
    char escaped[2048] = {};
    cyd::desktop::json::escape(traceback, escaped, sizeof(escaped));
    char response[2200];
    const int written = std::snprintf(response, sizeof(response),
        "{\"id\":\"%s\",\"ok\":true,\"result\":{\"app_id\":\"%s\",\"traceback\":\"%s\"}}",
        id, app_id, escaped);
    if (cyd::desktop::json::response_truncated(written, sizeof(response))) {
        error(id, "RESPONSE_TOO_LARGE", "app error did not fit the response buffer");
        return;
    }
    write_line(response);
}

bool sha256_hex(const char *value) {
    if (std::strlen(value) != 64) return false;
    for (size_t index = 0; index < 64; ++index) {
        if (!std::isxdigit(static_cast<unsigned char>(value[index]))) return false;
    }
    return true;
}

void handle_line(const char *line) {
    char id[64] = "";
    char command[64] = "";
    if (!json_string(line, "id", id, sizeof(id)) && cyd::desktop::json::has_key(line, "id")) {
        // The id was present but too long or not a string. Echoing a truncated
        // copy would leave the client unable to pair the reply with its
        // request, so say what is wrong instead of answering under a wrong id.
        error("", "INVALID_ARGUMENT", "id must be a string of 63 bytes or fewer");
        return;
    }
    if (!json_string(line, "command", command, sizeof(command))) {
        error(id, "INVALID_REQUEST", "command is required");
        return;
    }
    if (std::strcmp(command, "desktop_status") == 0) {
        status(id);
    } else if (std::strcmp(command, "desktop_ui_tree") == 0) {
        long long offset = 0, limit = 4;
        json_integer(line, "offset", &offset);
        json_integer(line, "limit", &limit);
        if (offset < 0 || limit < 1) error(id, "INVALID_ARGUMENT", "offset and limit must be positive");
        else ui_tree_page(id, static_cast<unsigned>(offset), static_cast<unsigned>(limit));
    } else if (std::strcmp(command, "desktop_installed_apps_list") == 0) {
        long long offset = 0, limit = 6;
        json_integer(line, "offset", &offset);
        json_integer(line, "limit", &limit);
        if (offset < 0 || limit < 1) error(id, "INVALID_ARGUMENT", "offset and limit must be positive");
        else installed_apps_page(id, static_cast<unsigned>(offset), static_cast<unsigned>(limit));
    } else if (std::strcmp(command, "desktop_apps_list") == 0) {
        char response[384];
        std::snprintf(response, sizeof(response),
                      "{\"id\":\"%s\",\"ok\":true,\"result\":{\"apps\":[\"clock\",\"calendar\",\"scripts\",\"settings\",\"neon3d\"]}}", id);
        write_line(response);
    } else if (std::strcmp(command, "desktop_transport_baud") == 0) {
        long long baud = 0;
        if (!json_integer(line, "baud", &baud) ||
            (baud != 115200 && baud != 460800 && baud != 921600)) {
            error(id, "INVALID_ARGUMENT", "baud must be 115200, 460800, or 921600");
            return;
        }
        char response[160];
        std::snprintf(response, sizeof(response),
            "{\"id\":\"%s\",\"ok\":true,\"result\":{\"baud\":%ld}}", id, static_cast<long>(baud));
        write_line(response);
        uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(500));
        vTaskDelay(pdMS_TO_TICKS(50));
        uart_set_baudrate(UART_NUM_0, static_cast<uint32_t>(baud));
    } else if (std::strcmp(command, "desktop_launch") == 0 || std::strcmp(command, "desktop_activate") == 0) {
        char app[32] = "";
        char worker_error[96] = {};
        if (!json_string(line, "app_id", app, sizeof(app))) {
            error(id, "INVALID_ARGUMENT", "unknown app_id");
            return;
        }
        if (!launch(app)) {
            if (!cyd_desktop_sd_mounted()) {
                error(id, "SD_UNAVAILABLE", "microSD is not mounted");
                return;
            }
            if (!safe_component(app, 24)) {
                error(id, "INVALID_ARGUMENT", "invalid app_id");
                return;
            }
            if (!script_launch(app, worker_error, sizeof(worker_error))) {
                error(id, "SCRIPT_ERROR", worker_error);
                return;
            }
        }
        cyd_desktop_ui_refresh();
        status(id);
    } else if (std::strcmp(command, "desktop_home") == 0) {
        cyd::desktop::shell_state().dispatch(cyd::desktop::Action::GoHome);
        cyd_desktop_ui_refresh();
        status(id);
    } else if (std::strcmp(command, "desktop_run") == 0) {
        long long frames = 0;
        if (!json_integer(line, "frames", &frames) || frames < 1 || frames > 3600) {
            error(id, "INVALID_ARGUMENT", "frames must be 1..3600");
            return;
        }
        bool attached = false, paused = false;
        uint32_t frame = 0, pending = 0;
        cyd_desktop_runtime_control_state(&attached, &paused, &frame, &pending);
        if (!attached) {
            error(id, "RUNTIME_INACTIVE", "no MicroPython app is running");
            return;
        }
        cyd_desktop_runtime_pause();
        cyd_desktop_runtime_control_state(&attached, &paused, &frame, &pending);
        if (!cyd_desktop_runtime_step(static_cast<uint32_t>(frames))) {
            error(id, "RUNTIME_ERROR", "could not grant frames");
            return;
        }
        char response[288];
        std::snprintf(response, sizeof(response),
                      "{\"id\":\"%s\",\"ok\":true,\"result\":{\"granted\":%ld,\"start_frame\":%lu,\"target_frame\":%lu,\"paused\":true}}",
                      id, static_cast<long>(frames), static_cast<unsigned long>(frame),
                      static_cast<unsigned long>(frame + frames));
        write_line(response);
    } else if (std::strcmp(command, "desktop_runtime_status") == 0
               || std::strcmp(command, "desktop_pause") == 0
               || std::strcmp(command, "desktop_resume") == 0
               || std::strcmp(command, "desktop_step") == 0) {
        bool attached = false, paused = false;
        uint32_t frame = 0, pending = 0;
        cyd_desktop_runtime_control_state(&attached, &paused, &frame, &pending);
        if (std::strcmp(command, "desktop_pause") == 0 && attached) cyd_desktop_runtime_pause();
        if (std::strcmp(command, "desktop_resume") == 0 && attached) cyd_desktop_runtime_resume();
        if (std::strcmp(command, "desktop_step") == 0 && attached) {
            cyd_desktop_runtime_pause();
            cyd_desktop_runtime_step(1);
        }
        cyd_desktop_runtime_control_state(&attached, &paused, &frame, &pending);
        char response[256];
        std::snprintf(response, sizeof(response),
            "{\"id\":\"%s\",\"ok\":true,\"result\":{\"attached\":%s,\"paused\":%s,\"frame\":%lu,\"pending_steps\":%lu}}",
            id, attached ? "true" : "false", paused ? "true" : "false",
            static_cast<unsigned long>(frame), static_cast<unsigned long>(pending));
        write_line(response);
    } else if (std::strcmp(command, "desktop_tap") == 0) {
        long long x = -1, y = -1;
        if (!json_integer(line, "x", &x) || !json_integer(line, "y", &y)
            || x < 0 || x >= 320 || y < 0 || y >= 240) {
            error(id, "INVALID_ARGUMENT", "x must be 0..319 and y must be 0..239");
            return;
        }
        if (!cyd_desktop_touch_inject(static_cast<int>(x), static_cast<int>(y))) {
            error(id, "INPUT_QUEUE_FULL", "synthetic input queue is full");
            return;
        }
        char response[160];
        std::snprintf(response, sizeof(response),
            "{\"id\":\"%s\",\"ok\":true,\"result\":{\"queued\":true,\"x\":%ld,\"y\":%ld}}",
            id, static_cast<long>(x), static_cast<long>(y));
        write_line(response);
    } else if (std::strcmp(command, "desktop_input_state") == 0) {
        bool pressed = false;
        int x = 0, y = 0;
        unsigned pending = 0;
        cyd_desktop_touch_state(&pressed, &x, &y, &pending);
        char response[192];
        std::snprintf(response, sizeof(response),
            "{\"id\":\"%s\",\"ok\":true,\"result\":{\"physical_pressed\":%s,\"x\":%d,\"y\":%d,\"pending_taps\":%u}}",
            id, pressed ? "true" : "false", x, y, pending);
        write_line(response);
    } else if (std::strcmp(command, "desktop_input_read") == 0 || std::strcmp(command, "desktop_input_clear") == 0) {
        char event[40] = {};
        const bool clear = std::strcmp(command, "desktop_input_clear") == 0;
        const bool has_event = cyd_desktop_input_read(event, sizeof(event), clear);
        char response[192];
        if (has_event) {
            std::snprintf(response, sizeof(response), "{\"id\":\"%s\",\"ok\":true,\"result\":{\"events\":[\"%s\"]}}", id, event);
        } else {
            std::snprintf(response, sizeof(response), "{\"id\":\"%s\",\"ok\":true,\"result\":{\"events\":[]}}", id);
        }
        write_line(response);
    } else if (std::strcmp(command, "desktop_diagnostics") == 0) {
        // {"fingerprint":1} starts summing the CRC of everything sent to the
        // LCD (from zero), {"fingerprint":0} stops it.
        long long fingerprint_request = -1;
        if (json_integer(line, "fingerprint", &fingerprint_request) && fingerprint_request >= 0)
            cyd_desktop_render_fingerprint_enable(fingerprint_request != 0);
        uint32_t renders = 0, transactions = 0, injected = 0, fingerprint = 0;
        uint64_t bytes = 0;
        bool attached = false, paused = false;
        uint32_t frame = 0, pending = 0;
        const auto native_metrics = cyd::desktop::native::metrics();
        cyd_desktop_render_metrics(&renders, &transactions, &bytes, &injected, &fingerprint);
        cyd_desktop_runtime_control_state(&attached, &paused, &frame, &pending);
        char response[720];
        std::snprintf(response, sizeof(response),
                      "{\"id\":\"%s\",\"ok\":true,\"result\":{\"transport\":\"usb-jsonl\",\"touch_calibrated\":%s,\"single_foreground\":true,\"heap_free\":%lu,\"heap_min\":%lu,\"task_count\":%u,\"render_count\":%lu,\"spi_transactions\":%lu,\"spi_bytes\":%lu,\"spi_fingerprint_enabled\":%s,\"spi_fingerprint\":%lu,\"synthetic_taps\":%lu,\"runtime_watchdog_ready\":%s,\"runtime_watchdog_attached\":%s,\"runtime_watchdog_error\":%d,\"runtime_paused\":%s,\"runtime_frame\":%lu,\"pending_steps\":%lu,\"native_frames\":%lu,\"native_last_frame_ms\":%lu,\"native_average_frame_ms\":%lu}}",
                      id, cyd_desktop_touch_calibrated() ? "true" : "false",
                      static_cast<unsigned long>(esp_get_free_heap_size()),
                      static_cast<unsigned long>(esp_get_minimum_free_heap_size()),
                      static_cast<unsigned>(uxTaskGetNumberOfTasks()),
                      static_cast<unsigned long>(renders), static_cast<unsigned long>(transactions),
                      static_cast<unsigned long>(bytes),
                      cyd_desktop_render_fingerprint_enabled() ? "true" : "false",
                      static_cast<unsigned long>(fingerprint),
                      static_cast<unsigned long>(injected),
                      cyd_desktop_runtime_watchdog_ready() ? "true" : "false",
                      attached ? "true" : "false", cyd_desktop_runtime_watchdog_error(),
                      paused ? "true" : "false", static_cast<unsigned long>(frame),
                      static_cast<unsigned long>(pending),
                      static_cast<unsigned long>(native_metrics.frames),
                      static_cast<unsigned long>(native_metrics.last_frame_ms),
                      static_cast<unsigned long>(native_metrics.average_frame_ms));
        write_line(response);
    } else if (std::strcmp(command, "desktop_logs") == 0) {
        long long offset = 0, limit = 4;
        json_integer(line, "offset", &offset);
        json_integer(line, "limit", &limit);
        if (offset < 0 || limit < 1) error(id, "INVALID_ARGUMENT", "offset and limit must be positive");
        else event_log_page(id, static_cast<unsigned>(offset), static_cast<unsigned>(limit));
    } else if (std::strcmp(command, "desktop_app_error") == 0) {
        app_error_report(id);
    } else if (std::strcmp(command, "desktop_settings_get") == 0 || std::strcmp(command, "desktop_settings_set") == 0) {
        if (std::strcmp(command, "desktop_settings_set") == 0) {
            long long brightness = -1, theme = -1, animation = -1, layout = -1;
            json_integer(line, "brightness", &brightness);
            json_integer(line, "theme", &theme);
            json_integer(line, "animation", &animation);
            json_integer(line, "keyboard_layout", &layout);
            if ((brightness < -1 || brightness > 2) || (theme < -1 || theme > 2)
                || (animation < -1 || animation > 2) || (layout < -1 || layout > 1)
                || (brightness < 0 && theme < 0 && animation < 0 && layout < 0)) {
                error(id, "INVALID_ARGUMENT", "settings values must be 0..2 (keyboard_layout 0..1)");
                return;
            }
            const bool display = brightness >= 0 || theme >= 0 || animation >= 0;
            if ((display && !cyd_desktop_settings_set(static_cast<int>(brightness), static_cast<int>(theme),
                                                      static_cast<int>(animation))) ||
                (layout >= 0 && !cyd_desktop_keyboard_layout_set(static_cast<int>(layout)))) {
                error(id, "SETTINGS_ERROR", "could not persist settings");
                return;
            }
            cyd_desktop_ui_refresh();
        }
        int brightness = 0, theme = 0, animation = 0;
        cyd_desktop_settings_get(&brightness, &theme, &animation);
        char response[192];
        std::snprintf(response, sizeof(response),
            "{\"id\":\"%s\",\"ok\":true,\"result\":{\"brightness\":%d,\"theme\":%d,\"animation\":%d,"
            "\"keyboard_layout\":%d}}",
            id, brightness, theme, animation, cyd_desktop_keyboard_layout());
        write_line(response);
    } else if (std::strcmp(command, "desktop_touch_calibrate_start") == 0) {
        cyd_desktop_touch_calibration_start();
        char response[128];
        std::snprintf(response, sizeof(response), "{\"id\":\"%s\",\"ok\":true,\"result\":{\"step\":1,\"total_steps\":3}}", id);
        write_line(response);
    } else if (std::strcmp(command, "desktop_sd_mount") == 0 || std::strcmp(command, "desktop_sd_status") == 0) {
        if (std::strcmp(command, "desktop_sd_mount") == 0) cyd_desktop_sd_mount();
        char response[192];
        std::snprintf(response, sizeof(response),
                      "{\"id\":\"%s\",\"ok\":true,\"result\":{\"mounted\":%s,\"last_error\":%d,\"boot_error\":%d}}",
                      id, cyd_desktop_sd_mounted() ? "true" : "false", cyd_desktop_sd_last_error(), cyd_desktop_sd_boot_error());
        write_line(response);
    } else if (std::strcmp(command, "desktop_package_begin") == 0 ||
               std::strcmp(command, "desktop_package_replace_begin") == 0) {
        char app_id[32] = {};
        char filename[32] = {};
        char sha256[65] = {};
        long long size = 0;
        char worker_error[96] = {};
        if (!cyd_desktop_sd_mounted()) {
            error(id, "SD_UNAVAILABLE", "microSD is not mounted");
        } else if (!json_string(line, "app_id", app_id, sizeof(app_id)) || !safe_component(app_id, 24)
                   || !json_string(line, "file", filename, sizeof(filename)) || !safe_component(filename, 24)
                   || !json_string(line, "sha256", sha256, sizeof(sha256)) || !sha256_hex(sha256)
                   || !json_integer(line, "size", &size) || size < 0 || size > 128 * 1024 * 1024) {
            error(id, "INVALID_ARGUMENT", "invalid package metadata");
        } else if (!cyd_desktop_package_submit(
                       std::strcmp(command, "desktop_package_replace_begin") == 0 ? 8 : 0,
                       app_id, filename, "", static_cast<uint32_t>(size), sha256,
                                                worker_error, sizeof(worker_error))) {
            error(id, "PACKAGE_ERROR", worker_error);
        } else {
            char response[128];
            std::snprintf(response, sizeof(response), "{\"id\":\"%s\",\"ok\":true,\"result\":{\"accepted\":true}}", id);
            write_line(response);
        }
    } else if (std::strcmp(command, "desktop_package_chunk") == 0) {
        char payload[2500] = {};
        char worker_error[96] = {};
        if (!json_string(line, "data", payload, sizeof(payload)) || payload[0] == '\0') {
            error(id, "INVALID_ARGUMENT", "data is required");
        } else if (!cyd_desktop_package_submit(1, "", "", payload, 0, "", worker_error, sizeof(worker_error))) {
            error(id, "PACKAGE_ERROR", worker_error);
        } else {
            char response[128];
            std::snprintf(response, sizeof(response), "{\"id\":\"%s\",\"ok\":true,\"result\":{\"accepted\":true}}", id);
            write_line(response);
        }
    } else if (std::strcmp(command, "desktop_package_finish") == 0) {
        char worker_error[96] = {};
        if (!cyd_desktop_package_submit(2, "", "", "", 0, "", worker_error, sizeof(worker_error))) {
            error(id, "PACKAGE_ERROR", worker_error);
        } else {
            char response[128];
            std::snprintf(response, sizeof(response), "{\"id\":\"%s\",\"ok\":true,\"result\":{\"committed\":true}}", id);
            write_line(response);
        }
    } else if (std::strcmp(command, "desktop_shortcuts_list") == 0) {
        long long offset = 0, limit = 6;
        json_integer(line, "offset", &offset);
        json_integer(line, "limit", &limit);
        if (offset < 0 || limit < 1) error(id, "INVALID_ARGUMENT", "offset and limit must be positive");
        else shortcuts_list(id, static_cast<unsigned>(offset), static_cast<unsigned>(limit));
    } else if (std::strcmp(command, "desktop_shortcut_create") == 0 || std::strcmp(command, "desktop_shortcut_update") == 0) {
        char shortcut_id[32] = {};
        char app_id[32] = {};
        char title[20] = {};
        char worker_error[96] = {};
        const bool is_update = std::strcmp(command, "desktop_shortcut_update") == 0;
        if (!cyd_desktop_sd_mounted()) {
            error(id, "SD_UNAVAILABLE", "microSD is not mounted");
        } else if (!json_string(line, "shortcut_id", shortcut_id, sizeof(shortcut_id)) || !safe_component(shortcut_id, 24)
                   || !json_string(line, "app_id", app_id, sizeof(app_id)) || !safe_component(app_id, 24)
                   || !json_string(line, "title", title, sizeof(title)) || !safe_title(title)) {
            error(id, "INVALID_ARGUMENT", "invalid shortcut metadata");
        } else if (!cyd_desktop_package_submit(is_update ? 5 : 4, shortcut_id, app_id, title, 0, "", worker_error, sizeof(worker_error))) {
            error(id, "SHORTCUT_ERROR", worker_error);
        } else {
            cyd_desktop_ui_refresh();
            char response[192];
            std::snprintf(response, sizeof(response), "{\"id\":\"%s\",\"ok\":true,\"result\":{\"saved\":true,\"shortcut_id\":\"%s\"}}", id, shortcut_id);
            write_line(response);
        }
    } else if (std::strcmp(command, "desktop_shortcut_delete") == 0) {
        char shortcut_id[32] = {};
        char worker_error[96] = {};
        if (!cyd_desktop_sd_mounted()) {
            error(id, "SD_UNAVAILABLE", "microSD is not mounted");
        } else if (!json_string(line, "shortcut_id", shortcut_id, sizeof(shortcut_id)) || !safe_component(shortcut_id, 24)) {
            error(id, "INVALID_ARGUMENT", "invalid shortcut_id");
        } else if (!cyd_desktop_package_submit(6, shortcut_id, "", "", 0, "", worker_error, sizeof(worker_error))) {
            error(id, "SHORTCUT_ERROR", worker_error);
        } else {
            cyd_desktop_ui_refresh();
            char response[192];
            std::snprintf(response, sizeof(response), "{\"id\":\"%s\",\"ok\":true,\"result\":{\"deleted\":true,\"shortcut_id\":\"%s\"}}", id, shortcut_id);
            write_line(response);
        }
    } else if (std::strcmp(command, "desktop_app_delete") == 0) {
        char app_id[32] = {};
        char worker_error[96] = {};
        if (!cyd_desktop_sd_mounted()) {
            error(id, "SD_UNAVAILABLE", "microSD is not mounted");
        } else if (!json_string(line, "app_id", app_id, sizeof(app_id)) || !safe_component(app_id, 24)) {
            error(id, "INVALID_ARGUMENT", "invalid app_id");
        } else if (!cyd_desktop_package_submit(7, app_id, "", "", 0, "", worker_error, sizeof(worker_error))) {
            error(id, "APP_DELETE_ERROR", worker_error);
        } else {
            cyd_desktop_ui_refresh();
            char response[160];
            std::snprintf(response, sizeof(response),
                "{\"id\":\"%s\",\"ok\":true,\"result\":{\"deleted\":true,\"app_id\":\"%s\"}}", id, app_id);
            write_line(response);
        }
    } else if (std::strcmp(command, "desktop_script_launch") == 0) {
        char app_id[32] = {};
        char worker_error[96] = {};
        if (!cyd_desktop_sd_mounted()) {
            error(id, "SD_UNAVAILABLE", "microSD is not mounted");
        } else if (!json_string(line, "app_id", app_id, sizeof(app_id)) || !safe_component(app_id, 24)) {
            error(id, "INVALID_ARGUMENT", "invalid app_id");
        } else if (!script_launch(app_id, worker_error, sizeof(worker_error))) {
            error(id, "SCRIPT_ERROR", worker_error);
        } else {
            char response[160];
            std::snprintf(response, sizeof(response), "{\"id\":\"%s\",\"ok\":true,\"result\":{\"started\":true,\"app_id\":\"%s\"}}", id, app_id);
            write_line(response);
        }
    } else if (std::strcmp(command, "desktop_radio_status") == 0 || std::strcmp(command, "desktop_radio_set") == 0) {
        // Wi-Fi and Bluetooth are exclusive; a new mode applies after a restart.
        const bool set = std::strcmp(command, "desktop_radio_set") == 0;
        char mode_name[16] = {};
        long long restart = 0;
        int mode = -1;
        if (set) {
            json_string(line, "mode", mode_name, sizeof(mode_name));
            json_integer(line, "restart", &restart);
            for (int candidate = 0; candidate <= 2; ++candidate) {
                if (std::strcmp(mode_name, cyd_desktop_radio_mode_name(candidate)) == 0) mode = candidate;
            }
        }
        if (set && mode < 0) {
            error(id, "INVALID_ARGUMENT", "mode must be off, wifi or bluetooth");
        } else if (set && cyd_desktop_runtime_watchdog_attached() && restart != 0) {
            error(id, "APP_RUNNING", "stop the foreground app before restarting");
        } else if (set && !cyd_desktop_radio_set_next_mode(mode)) {
            error(id, "NVS_ERROR", "could not store radio_mode");
        } else {
            const int running = cyd_desktop_radio_mode();
            const int next = cyd_desktop_radio_next_mode();
            char response[200];
            std::snprintf(response, sizeof(response),
                "{\"id\":\"%s\",\"ok\":true,\"result\":{\"mode\":\"%s\",\"next_mode\":\"%s\","
                "\"restart_required\":%s,\"restarting\":%s}}",
                id, cyd_desktop_radio_mode_name(running), cyd_desktop_radio_mode_name(next),
                running != next ? "true" : "false", set && restart != 0 ? "true" : "false");
            write_line(response);
            if (set && restart != 0) cyd_desktop_radio_restart();
        }
    } else if (std::strcmp(command, "desktop_bt_status") == 0 || std::strcmp(command, "desktop_bt_memory") == 0) {
        // Bluetooth keyboard pairing (radio mode bluetooth) and memory diagnostics.
        char body[1100];
        if (std::strcmp(command, "desktop_bt_status") == 0) cyd_desktop_bt_status_json(body, sizeof(body));
        else cyd_desktop_bt_memory_json(body, sizeof(body));
        char response[1200];
        std::snprintf(response, sizeof(response), "{\"id\":\"%s\",\"ok\":true,\"result\":%s}", id, body);
        write_line(response);
    } else if (std::strcmp(command, "desktop_bt_scan") == 0 || std::strcmp(command, "desktop_bt_connect") == 0 ||
               std::strcmp(command, "desktop_bt_forget") == 0) {
        if (cyd_desktop_radio_mode() != CYD_RADIO_BLUETOOTH) {
            error(id, "BT_UNAVAILABLE", "Bluetooth is not running: select Bluetooth in Settings > Wireless");
            return;
        }
        if (std::strcmp(command, "desktop_bt_scan") == 0) {
            long long seconds = 10;
            json_integer(line, "seconds", &seconds);
            if (seconds < 1 || seconds > 30) {
                error(id, "INVALID_ARGUMENT", "seconds must be 1..30");
                return;
            }
            if (!cyd_desktop_bt_scan(static_cast<int>(seconds))) {
                error(id, "BT_BUSY", "could not start a scan");
                return;
            }
        } else if (std::strcmp(command, "desktop_bt_connect") == 0) {
            long long index = -1;
            if (!json_integer(line, "index", &index) || index < 0 || index >= CYD_BT_MAX_FOUND ||
                !cyd_desktop_bt_connect(static_cast<int>(index))) {
                error(id, "INVALID_ARGUMENT", "index must name a result of the last scan");
                return;
            }
        } else {
            cyd_desktop_bt_forget();
        }
        cyd_desktop_ui_refresh_async();
        char body[1100];
        cyd_desktop_bt_status_json(body, sizeof(body));
        char response[1200];
        std::snprintf(response, sizeof(response), "{\"id\":\"%s\",\"ok\":true,\"result\":%s}", id, body);
        write_line(response);
    } else if (std::strcmp(command, "desktop_key") == 0) {
        // One key as apps name it ("a", "ENTER", "CTRL+S"), routed like a keyboard press.
        char name[24] = {};
        if (!json_string(line, "key", name, sizeof(name)) || name[0] == '\0') {
            error(id, "INVALID_ARGUMENT", "key is required");
        } else if (!cyd_desktop_key_inject(name)) {
            error(id, "INVALID_ARGUMENT", "unknown key name, or the key queue is full");
        } else {
            char escaped[64];
            json_escape(name, escaped, sizeof(escaped));
            char response[160];
            std::snprintf(response, sizeof(response), "{\"id\":\"%s\",\"ok\":true,\"result\":{\"key\":\"%s\"}}",
                          id, escaped);
            write_line(response);
        }
    } else if (std::strcmp(command, "desktop_time_set") == 0) {
        long long epoch = 0;
        if (!json_integer(line, "epoch", &epoch) || epoch < 946684800LL) {
            error(id, "INVALID_ARGUMENT", "epoch must be Unix seconds since 2000-01-01");
            return;
        }
        timeval clock{static_cast<time_t>(epoch - kUnixEpochAtYear2000), 0};
        settimeofday(&clock, nullptr);
        status(id);
    } else {
        error(id, "UNKNOWN_COMMAND", "unsupported desktop command");
    }
}

void protocol_task(void *) {
    setenv("TZ", "JST-9", 1);
    tzset();
    uart_config_t config{};
    config.baud_rate = 115200;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;
    uart_param_config(UART_NUM_0, &config);
    uart_set_pin(UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (uart_driver_install(UART_NUM_0, 2048, 0, 0, nullptr, 0) != ESP_OK) {
        vTaskDelete(nullptr);
    }
    char line[kLineLimit + 1]{};
    bool overflowed = false;
    size_t length = 0;
    while (true) {
        uint8_t byte = 0;
        const int received = uart_read_bytes(UART_NUM_0, &byte, 1, pdMS_TO_TICKS(100));
        if (received != 1) continue;
        if (byte == '\n' || byte == '\r') {
            if (overflowed) {
                // Report the overflow once, at the end of the offending line,
                // so the discarded tail is never parsed as a second request.
                error("", "OUT_OF_RANGE", "request exceeded the line length limit");
                overflowed = false;
                length = 0;
            } else if (length > 0) {
                line[length] = '\0';
                const char *start = line;
                while (*start == ' ' || *start == '\t') ++start;
                if (*start == '{') {
                    handle_line(start);
                } else {
                    // Silence here used to look like a hung device: a client
                    // that pretty-printed its JSON, or sent anything else at
                    // all, simply waited out its timeout.
                    error("", "INVALID_REQUEST", "expected one JSON object per line");
                }
                length = 0;
            }
        } else if (overflowed) {
            continue;  // discard the remainder of the oversize line
        } else if (length < kLineLimit) {
            line[length++] = static_cast<char>(byte);
        } else {
            overflowed = true;
            length = 0;
        }
    }
}

}  // namespace

extern "C" void cyd_desktop_protocol_start(void) {
    if (protocol_started) return;
    protocol_started = true;
    xTaskCreatePinnedToCore(protocol_task, "cyd-jsonl", 16384, nullptr, 6, nullptr, tskNO_AFFINITY);
}
