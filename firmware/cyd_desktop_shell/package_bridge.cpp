#include <cstring>
#include <cstdio>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "py/runtime.h"
}

extern "C" bool cyd_desktop_runtime_watchdog_attached(void);

namespace {

enum class PackageOp : uint8_t {
    Begin = 0, Chunk = 1, Finish = 2, Run = 3,
    ShortcutCreate = 4, ShortcutUpdate = 5, ShortcutDelete = 6, AppDelete = 7,
    BeginReplace = 8, FileRead = 9, FileWrite = 10, FileList = 11, AppsRescan = 12,
};

struct PackageRequest {
    uint32_t ticket;
    PackageOp operation;
    char app_id[64];
    char filename[32];
    char payload[2500];
    uint32_t expected_size;
    char sha256[65];
    bool reply_requested;
};

QueueHandle_t requests = nullptr;
// Held by one submitter at a time: the request copy lives in static storage.
SemaphoreHandle_t submit_lock = nullptr;
SemaphoreHandle_t completed = nullptr;
bool response_ok = false;
char response_error[80] = {};
// active_ticket is the request the worker dequeued and is executing; the
// worker is single threaded, so its next package_result belongs to that one.
// A caller compares it against its own ticket and rejects a reply that was
// really the answer to an earlier request it had already timed out on.
uint32_t active_ticket = 0;
uint32_t response_ticket = 0;
uint32_t next_ticket = 1;

void ensure_bridge() {
    // Each request is about 2.7 KB and submitters wait for the worker, so one slot is enough.
    if (requests == nullptr) requests = xQueueCreate(1, sizeof(PackageRequest));
    if (completed == nullptr) completed = xSemaphoreCreateBinary();
    if (submit_lock == nullptr) submit_lock = xSemaphoreCreateMutex();
}

void copy_string(char *destination, size_t capacity, const char *source) {
    std::snprintf(destination, capacity, "%s", source == nullptr ? "" : source);
}

mp_obj_t package_next() {
    ensure_bridge();
    PackageRequest request{};
    if (xQueueReceive(requests, &request, pdMS_TO_TICKS(100)) != pdTRUE) return mp_const_none;
    active_ticket = request.ticket;
    const char *operation = request.operation == PackageOp::Begin ? "begin"
        : request.operation == PackageOp::BeginReplace ? "begin_replace"
        : request.operation == PackageOp::Chunk ? "chunk"
        : request.operation == PackageOp::Finish ? "finish"
        : request.operation == PackageOp::Run ? "run"
        : request.operation == PackageOp::ShortcutCreate ? "shortcut_create"
        : request.operation == PackageOp::ShortcutUpdate ? "shortcut_update"
        : request.operation == PackageOp::ShortcutDelete ? "shortcut_delete"
        : request.operation == PackageOp::FileRead ? "file_read"
        : request.operation == PackageOp::FileWrite ? "file_write"
        : request.operation == PackageOp::FileList ? "file_list"
        : request.operation == PackageOp::AppsRescan ? "apps_rescan" : "app_delete";
    mp_obj_t values[] = {
        mp_obj_new_str(operation, std::strlen(operation)),
        mp_obj_new_str(request.app_id, std::strlen(request.app_id)),
        mp_obj_new_str(request.filename, std::strlen(request.filename)),
        mp_obj_new_str(request.payload, std::strlen(request.payload)),
        mp_obj_new_int_from_uint(request.expected_size),
        mp_obj_new_str(request.sha256, std::strlen(request.sha256)),
        mp_obj_new_bool(request.reply_requested),
    };
    return mp_obj_new_tuple(7, values);
}
static MP_DEFINE_CONST_FUN_OBJ_0(package_next_obj, package_next);

mp_obj_t package_result(size_t n_args, const mp_obj_t *args) {
    ensure_bridge();
    response_ticket = active_ticket;
    response_ok = mp_obj_is_true(args[0]);
    size_t length = 0;
    const char *message = mp_obj_str_get_data(args[1], &length);
    const size_t used = length < sizeof(response_error) - 1 ? length : sizeof(response_error) - 1;
    std::memcpy(response_error, message, used);
    response_error[used] = '\0';
    xSemaphoreGive(completed);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(package_result_obj, 2, 2, package_result);

}  // namespace

extern "C" mp_obj_t cyd_desktop_package_next_py(void) { return package_next(); }
extern "C" mp_obj_t cyd_desktop_package_result_py(size_t n_args, const mp_obj_t *args) { return package_result(n_args, args); }

namespace {

bool submit_locked(int operation, const char *app_id, const char *filename,
    const char *payload, uint32_t expected_size, const char *sha256, char *error, size_t error_capacity) {
    // The request is 2.7 KB. Submitters include the UI task (editor save), whose
    // stack cannot spare that; the queue now holds one request, so this is no
    // extra RAM overall.
    static PackageRequest request;
    request = PackageRequest{};
    request.ticket = next_ticket++;
    if (next_ticket == 0) next_ticket = 1;
    response_ticket = 0;
    request.operation = static_cast<PackageOp>(operation);
    copy_string(request.app_id, sizeof(request.app_id), app_id);
    copy_string(request.filename, sizeof(request.filename), filename);
    copy_string(request.payload, sizeof(request.payload), payload);
    request.expected_size = expected_size;
    copy_string(request.sha256, sizeof(request.sha256), sha256);
    request.reply_requested = true;
    response_ok = false;
    response_error[0] = '\0';
    while (xSemaphoreTake(completed, 0) == pdTRUE) {}
    if (xQueueSend(requests, &request, pdMS_TO_TICKS(500)) != pdTRUE) {
        copy_string(error, error_capacity, "package worker is busy");
        return false;
    }
    const TickType_t timeout = request.operation == PackageOp::Finish
        ? pdMS_TO_TICKS(120000) : pdMS_TO_TICKS(5000);
    if (xSemaphoreTake(completed, timeout) != pdTRUE) {
        copy_string(error, error_capacity, "package worker timed out");
        return false;
    }
    if (response_ticket != request.ticket) {
        // A reply from an earlier request that had already timed out.
        copy_string(error, error_capacity, "package worker reply out of order");
        return false;
    }
    copy_string(error, error_capacity, response_error);
    return response_ok;
}

}  // namespace

extern "C" bool cyd_desktop_package_submit(int operation, const char *app_id, const char *filename,
    const char *payload, uint32_t expected_size, const char *sha256, char *error, size_t error_capacity) {
    ensure_bridge();
    if (cyd_desktop_runtime_watchdog_attached()) {
        copy_string(error, error_capacity, "foreground app is running");
        return false;
    }
    if (xSemaphoreTake(submit_lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        copy_string(error, error_capacity, "package worker is busy");
        return false;
    }
    const bool ok = submit_locked(operation, app_id, filename, payload, expected_size, sha256,
                                  error, error_capacity);
    xSemaphoreGive(submit_lock);
    return ok;
}

// Re-reads /sd/apps so Scripts lists apps copied to the card since boot. Does
// not wait: the worker repaints when it is done, and a busy worker skips it.
extern "C" bool cyd_desktop_apps_rescan_async(void) {
    ensure_bridge();
    if (cyd_desktop_runtime_watchdog_attached()) return false;
    PackageRequest request{};
    request.operation = PackageOp::AppsRescan;
    request.reply_requested = false;
    return xQueueSend(requests, &request, 0) == pdTRUE;
}

extern "C" bool cyd_desktop_script_launch_async(const char *app_id) {
    ensure_bridge();
    if (app_id == nullptr || app_id[0] == '\0') return false;
    PackageRequest request{};
    request.operation = PackageOp::Run;
    copy_string(request.app_id, sizeof(request.app_id), app_id);
    request.reply_requested = false;
    return xQueueSend(requests, &request, pdMS_TO_TICKS(20)) == pdTRUE;
}
