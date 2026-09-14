// Wi-Fi exists only in Wi-Fi radio mode, even for an app that bypasses cyd.
//
// cyd.wifi_*() check the radio mode, but an app can still `import network`
// and call network.WLAN(). Starting Wi-Fi next to Bluetooth, or after
// Bluetooth's memory was released, would take heap the rest of the system
// counts on. MicroPython's Wi-Fi start-up (esp_initialise_wifi in
// ports/esp32/network_wlan.c) first registers its WIFI_EVENT handler, before
// it allocates anything, so that call is wrapped (-Wl,--wrap, set in the
// cyd_bt_keyboard component) and refused outside Wi-Fi mode.
//
// ESP-NOW is the other way in: espnow.ESPNow().active(True) calls
// esp_now_init() without starting Wi-Fi, and with Wi-Fi never started that
// faults (LoadProhibited) and resets the board. It is refused the same way.

#include "bt_keyboard.h"

extern "C" {
#include "esp_event.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "py/runtime.h"
#include "mphalport.h"
}

#include <cstring>

namespace {

// Refuses when the radio mode is not Wi-Fi. Returns the error for callers off
// the MicroPython task; on it, raises OSError instead of returning.
esp_err_t refuse_without_wifi() {
    if (cyd_desktop_radio_mode() == CYD_RADIO_WIFI) return ESP_OK;
    if (xTaskGetCurrentTaskHandle() == mp_main_task_handle) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Wi-Fi is off: choose Wi-Fi in Settings > Wireless"));
    }
    return ESP_ERR_NOT_SUPPORTED;
}

}  // namespace

extern "C" esp_err_t __real_esp_now_init(void);
extern "C" esp_err_t __wrap_esp_now_init(void) {
    const esp_err_t refused = refuse_without_wifi();
    return refused != ESP_OK ? refused : __real_esp_now_init();
}

extern "C" esp_err_t __real_esp_event_handler_instance_register(esp_event_base_t event_base, int32_t event_id,
    esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *instance);

extern "C" esp_err_t __wrap_esp_event_handler_instance_register(esp_event_base_t event_base, int32_t event_id,
    esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *instance) {
    const bool wifi_event = event_base == WIFI_EVENT ||
                            (event_base != nullptr && std::strcmp(event_base, WIFI_EVENT) == 0);
    if (wifi_event) {
        const esp_err_t refused = refuse_without_wifi();
        if (refused != ESP_OK) return refused;
    }
    return __real_esp_event_handler_instance_register(event_base, event_id, event_handler, event_handler_arg, instance);
}
