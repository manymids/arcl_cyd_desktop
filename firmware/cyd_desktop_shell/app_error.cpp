// The last foreground app failure, kept in full for desktop_app_error.
//
// The app log line is 48 characters, which cut every traceback down to a type
// and the start of a message. _boot.py hands the whole formatted traceback here
// (and to /sd/desktop/errors/<app-id>.txt) so a client can read what happened.

#include <cstddef>
#include <cstring>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

namespace {

constexpr std::size_t kAppIdCapacity = 25;
constexpr std::size_t kTracebackCapacity = 1024;

char failed_app[kAppIdCapacity] = {};
char traceback_text[kTracebackCapacity] = {};
portMUX_TYPE error_lock = portMUX_INITIALIZER_UNLOCKED;

std::size_t bounded_copy(char *destination, std::size_t capacity, const char *source) {
    if (capacity == 0) return 0;
    std::size_t length = source == nullptr ? 0 : std::strlen(source);
    if (length >= capacity) length = capacity - 1;
    if (length > 0) std::memcpy(destination, source, length);
    destination[length] = '\0';
    return length;
}

}  // namespace

extern "C" void cyd_desktop_app_error_set(const char *app_id, const char *text) {
    // Plain copies only inside the lock: no formatting while interrupts are off.
    taskENTER_CRITICAL(&error_lock);
    bounded_copy(failed_app, sizeof(failed_app), app_id);
    bounded_copy(traceback_text, sizeof(traceback_text), text);
    taskEXIT_CRITICAL(&error_lock);
}

extern "C" void cyd_desktop_app_error_get(char *app_id, std::size_t app_id_capacity,
                                         char *text, std::size_t text_capacity) {
    taskENTER_CRITICAL(&error_lock);
    bounded_copy(app_id, app_id_capacity, failed_app);
    bounded_copy(text, text_capacity, traceback_text);
    taskEXIT_CRITICAL(&error_lock);
}
