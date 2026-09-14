#include "screen/ui_registry.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace cyd::desktop::screen {

namespace {

struct UiNode {
    char id[41];
    char role[13];
    char label[25];
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    bool enabled;
};
constexpr uint8_t kUiNodeCapacity = 24;
UiNode ui_nodes[kUiNodeCapacity]{};
uint8_t ui_node_count = 0;
UiNode staging_ui_nodes[kUiNodeCapacity]{};
uint8_t staging_ui_node_count = 0;
bool ui_nodes_collecting = false;
portMUX_TYPE ui_nodes_lock = portMUX_INITIALIZER_UNLOCKED;

struct EventRecord { uint32_t tick_ms; char value[49]; };
EventRecord event_log[16]{};
uint8_t event_count = 0;
uint8_t event_head = 0;
portMUX_TYPE event_lock = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE input_lock = portMUX_INITIALIZER_UNLOCKED;
char last_input_event[kInputEventCapacity] = {};

}  // namespace

bool ui_nodes_suppressed = false;

void record_event(const char *value) {
    taskENTER_CRITICAL(&event_lock);
    auto &entry = event_log[event_head];
    entry.tick_ms = static_cast<uint32_t>(xTaskGetTickCount()) * portTICK_PERIOD_MS;
    std::snprintf(entry.value, sizeof(entry.value), "%s", value == nullptr ? "" : value);
    event_head = (event_head + 1) % 16;
    if (event_count < 16) ++event_count;
    taskEXIT_CRITICAL(&event_lock);
}

void record_input(const char *event) {
    taskENTER_CRITICAL(&input_lock);
    std::snprintf(last_input_event, sizeof(last_input_event), "%s", event);
    taskEXIT_CRITICAL(&input_lock);
    record_event(event);
}

void ui_nodes_begin() {
    taskENTER_CRITICAL(&ui_nodes_lock);
    staging_ui_node_count = 0;
    ui_nodes_collecting = true;
    taskEXIT_CRITICAL(&ui_nodes_lock);
}

void ui_nodes_end() {
    taskENTER_CRITICAL(&ui_nodes_lock);
    std::memcpy(ui_nodes, staging_ui_nodes, staging_ui_node_count * sizeof(UiNode));
    ui_node_count = staging_ui_node_count;
    ui_nodes_collecting = false;
    taskEXIT_CRITICAL(&ui_nodes_lock);
}

void ui_node(const char *id, const char *role, int x, int y, int width, int height,
             const char *label, bool enabled) {
    if (ui_nodes_suppressed) return;
    taskENTER_CRITICAL(&ui_nodes_lock);
    if (ui_nodes_collecting && staging_ui_node_count < kUiNodeCapacity) {
        auto &node = staging_ui_nodes[staging_ui_node_count++];
        std::snprintf(node.id, sizeof(node.id), "%s", id == nullptr ? "" : id);
        std::snprintf(node.role, sizeof(node.role), "%s", role == nullptr ? "" : role);
        std::snprintf(node.label, sizeof(node.label), "%s", label == nullptr ? "" : label);
        node.x = x; node.y = y; node.width = width; node.height = height; node.enabled = enabled;
    }
    taskEXIT_CRITICAL(&ui_nodes_lock);
}

}  // namespace cyd::desktop::screen

using namespace cyd::desktop::screen;

extern "C" unsigned cyd_desktop_ui_node_count(void) {
    taskENTER_CRITICAL(&ui_nodes_lock);
    const unsigned count = ui_node_count;
    taskEXIT_CRITICAL(&ui_nodes_lock);
    return count;
}

extern "C" bool cyd_desktop_ui_node_get(unsigned index, char *id, size_t id_capacity,
    char *role, size_t role_capacity, char *label, size_t label_capacity,
    int *x, int *y, int *width, int *height, bool *enabled) {
    taskENTER_CRITICAL(&ui_nodes_lock);
    if (index >= ui_node_count) {
        taskEXIT_CRITICAL(&ui_nodes_lock);
        return false;
    }
    const UiNode node = ui_nodes[index];
    taskEXIT_CRITICAL(&ui_nodes_lock);
    std::snprintf(id, id_capacity, "%s", node.id);
    std::snprintf(role, role_capacity, "%s", node.role);
    std::snprintf(label, label_capacity, "%s", node.label);
    if (x != nullptr) *x = node.x;
    if (y != nullptr) *y = node.y;
    if (width != nullptr) *width = node.width;
    if (height != nullptr) *height = node.height;
    if (enabled != nullptr) *enabled = node.enabled;
    return true;
}

extern "C" unsigned cyd_desktop_event_count(void) {
    taskENTER_CRITICAL(&event_lock);
    const unsigned count = event_count;
    taskEXIT_CRITICAL(&event_lock);
    return count;
}

extern "C" bool cyd_desktop_event_get(unsigned index, uint32_t *tick_ms, char *value, size_t capacity) {
    taskENTER_CRITICAL(&event_lock);
    if (index >= event_count) {
        taskEXIT_CRITICAL(&event_lock);
        return false;
    }
    const uint8_t oldest = (event_head + 16 - event_count) % 16;
    const EventRecord entry = event_log[(oldest + index) % 16];
    taskEXIT_CRITICAL(&event_lock);
    if (tick_ms != nullptr) *tick_ms = entry.tick_ms;
    std::snprintf(value, capacity, "%s", entry.value);
    return true;
}

extern "C" bool cyd_desktop_input_read(char *event, size_t capacity, bool clear) {
    if (capacity == 0) return false;
    taskENTER_CRITICAL(&input_lock);
    const bool has_event = last_input_event[0] != '\0';
    std::snprintf(event, capacity, "%s", last_input_event);
    if (clear) last_input_event[0] = '\0';
    taskEXIT_CRITICAL(&input_lock);
    return has_event;
}

extern "C" bool cyd_desktop_app_event(char *event, size_t capacity) {
    return cyd_desktop_input_read(event, capacity, true);
}
