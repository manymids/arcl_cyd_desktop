// Radio mode and the Bluetooth Classic keyboard host.
//
// The NVS key cyd_desktop/radio_mode (0 off, 1 Wi-Fi, 2 Bluetooth; default
// off) is read once at boot. Only in Bluetooth mode does Bluedroid start with
// the HID host; otherwise the controller's static memory is released to the
// heap, which cannot be undone, so changing the mode takes a restart. Wi-Fi
// and Bluetooth are exclusive because the heap cannot hold both.
//
// One keyboard at a time: pairing a new one forgets the previous bond. The
// remembered keyboard's name and address are kept in NVS (bt_name, bt_addr)
// because the bond list holds addresses only.
//
// The board stays connectable so a sleeping keyboard can reconnect, which also
// lets any nearby device ask to pair. A device that pairs becomes a keyboard,
// and keys reach the console, so pairing is accepted only for the device the
// user picked on the Keyboard page (or through desktop_bt_connect), within
// kPairingWindowUs of picking it. Links from anything else but the remembered
// keyboard are closed and their bonds removed.
//
// This file is built by the cyd_bt_keyboard IDF component, not the MicroPython
// user module (see that component's CMakeLists.txt). include/bt_keyboard.h is
// its interface.

#include "bt_keyboard.h"
#include "sdkconfig.h"
#include "json_buffer.h"
#include "screen/ui_registry.h"

extern "C" {
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
}

#include <cstdarg>
#include <cstdio>
#include <cstring>

#if CONFIG_BT_ENABLED && CONFIG_BT_CLASSIC_ENABLED && CONFIG_BT_HID_HOST_ENABLED
#define CYD_BT_SUPPORTED 1
extern "C" {
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hidh.h"
}
#else
#define CYD_BT_SUPPORTED 0
#endif

namespace {

using cyd::desktop::screen::record_event;

int stored_radio_mode() {
    nvs_handle_t handle;
    uint8_t mode = CYD_RADIO_OFF;
    if (nvs_open("cyd_desktop", NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_u8(handle, "radio_mode", &mode);
        nvs_close(handle);
    }
    return mode <= CYD_RADIO_BLUETOOTH ? static_cast<int>(mode) : static_cast<int>(CYD_RADIO_OFF);
}

// -1 until the first call reads NVS; after that, the mode of this boot.
int boot_radio_mode = -1;

[[maybe_unused]] void log_event(const char *format, ...) {
    char text[49];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    record_event(text);
}

[[maybe_unused]] bool started = false;
[[maybe_unused]] const char *start_error = "";

[[maybe_unused]] const char *pairing_name(int state) {
    switch (state) {
        case CYD_BT_PAIRING_CONNECTING: return "connecting";
        case CYD_BT_PAIRING_PIN: return "pin";
        case CYD_BT_PAIRING_PASSKEY: return "passkey";
        case CYD_BT_PAIRING_DONE: return "done";
        case CYD_BT_PAIRING_FAILED: return "failed";
        default: return "idle";
    }
}

#if CYD_BT_SUPPORTED

struct FoundDevice {
    esp_bd_addr_t bda;
    char name[32];
    int rssi;
    bool keyboard;
    bool name_requested;
};

// Everything below is shared between the Bluedroid tasks and the UI and
// protocol tasks, and guarded by `lock`. NVS and Bluedroid calls stay outside it.
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
uint32_t generation = 0;
FoundDevice found[CYD_BT_MAX_FOUND];
size_t found_count = 0;
bool discovering = false;
esp_hidh_dev_t *connected_dev = nullptr;
char connected_name[32] = {};
bool remembered = false;
esp_bd_addr_t remembered_bda = {};
char remembered_name[32] = {};
int pairing = CYD_BT_PAIRING_IDLE;
char pairing_code[8] = {};
esp_bd_addr_t target_bda = {};
char target_name[32] = {};
constexpr int64_t kPairingWindowUs = 60 * 1000000LL;
int64_t pairing_deadline_us = 0;
constexpr size_t kKeyQueueLength = 32;
constexpr int64_t kRepeatDelayUs = 500000;
constexpr int64_t kRepeatIntervalUs = 35000;
cyd_bt_key_t keys[kKeyQueueLength] = {};
size_t key_head = 0;
size_t key_count = 0;
uint8_t previous_keys[6] = {};
uint8_t held_usage = 0;
uint8_t held_modifiers = 0;
int64_t next_repeat_us = 0;

// Call with `lock` held.
void changed() { ++generation; }

void format_address(const uint8_t *bda, char *out, size_t capacity) {
    std::snprintf(out, capacity, "%02x:%02x:%02x:%02x:%02x:%02x",
                  bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}

void save_remembered(const uint8_t *bda, const char *name);

// Empty, or the address standing in for a name nobody has read yet.
bool unnamed(const char *name) {
    return name[0] == '\0' || (std::strlen(name) == 17 && name[2] == ':' && name[14] == ':');
}

bool same_address(const uint8_t *a, const uint8_t *b) {
    return std::memcmp(a, b, sizeof(esp_bd_addr_t)) == 0;
}

// Call with `lock` held. True while the user is pairing with exactly `bda`.
bool pairing_requested_for(const uint8_t *bda) {
    const bool in_progress = pairing == CYD_BT_PAIRING_CONNECTING || pairing == CYD_BT_PAIRING_PIN ||
                             pairing == CYD_BT_PAIRING_PASSKEY;
    return in_progress && esp_timer_get_time() < pairing_deadline_us && same_address(target_bda, bda);
}

// Call with `lock` held.
bool is_remembered(const uint8_t *bda) {
    return remembered && same_address(remembered_bda, bda);
}

// Some keyboards leave their name out of inquiry responses, so ask each
// unnamed scan result in turn. One request at a time, and never while pairing.
void request_next_name() {
    esp_bd_addr_t bda;
    bool found_one = false;
    taskENTER_CRITICAL(&lock);
    if (!discovering && pairing == CYD_BT_PAIRING_IDLE) {
        for (size_t index = 0; index < found_count && !found_one; ++index) {
            if (found[index].name[0] == '\0' && !found[index].name_requested) {
                found[index].name_requested = true;
                std::memcpy(bda, found[index].bda, sizeof(bda));
                found_one = true;
            }
        }
    }
    taskEXIT_CRITICAL(&lock);
    if (found_one) esp_bt_gap_read_remote_name(bda);
}

void handle_remote_name(const uint8_t *bda, bool ok, const uint8_t *remote_name) {
    char name[32] = {};
    if (ok) std::snprintf(name, sizeof(name), "%.31s", reinterpret_cast<const char *>(remote_name));
    bool save = false;
    taskENTER_CRITICAL(&lock);
    if (name[0] != '\0') {
        for (size_t index = 0; index < found_count; ++index) {
            if (same_address(found[index].bda, bda) && found[index].name[0] == '\0') {
                std::memcpy(found[index].name, name, sizeof(name));
            }
        }
        if (same_address(target_bda, bda) && unnamed(target_name)) std::memcpy(target_name, name, sizeof(name));
        if (remembered && same_address(remembered_bda, bda) && unnamed(remembered_name)) {
            std::memcpy(remembered_name, name, sizeof(name));
            if (connected_dev != nullptr) std::memcpy(connected_name, name, sizeof(name));
            save = true;
        }
        changed();
    }
    taskEXIT_CRITICAL(&lock);
    if (save) save_remembered(bda, name);
    log_event("bt.name %s", name[0] != '\0' ? name : "unknown");
    request_next_name();
}

void load_remembered() {
    nvs_handle_t handle;
    if (nvs_open("cyd_desktop", NVS_READONLY, &handle) != ESP_OK) return;
    esp_bd_addr_t bda;
    size_t length = sizeof(bda);
    char name[32] = {};
    size_t name_length = sizeof(name);
    const bool have = nvs_get_blob(handle, "bt_addr", bda, &length) == ESP_OK && length == sizeof(bda);
    if (have) nvs_get_str(handle, "bt_name", name, &name_length);
    nvs_close(handle);
    if (!have) return;
    taskENTER_CRITICAL(&lock);
    remembered = true;
    std::memcpy(remembered_bda, bda, sizeof(bda));
    std::memcpy(remembered_name, name, sizeof(name));
    remembered_name[sizeof(remembered_name) - 1] = '\0';
    changed();
    taskEXIT_CRITICAL(&lock);
}

void save_remembered(const uint8_t *bda, const char *name) {
    nvs_handle_t handle;
    if (nvs_open("cyd_desktop", NVS_READWRITE, &handle) != ESP_OK) return;
    if (bda != nullptr) {
        nvs_set_blob(handle, "bt_addr", bda, sizeof(esp_bd_addr_t));
        nvs_set_str(handle, "bt_name", name);
    } else {
        nvs_erase_key(handle, "bt_addr");
        nvs_erase_key(handle, "bt_name");
    }
    nvs_commit(handle);
    nvs_close(handle);
}

// Remove every bond except `keep` (nullptr removes all). Returns bonds removed.
int remove_bonds_except(const uint8_t *keep) {
    int count = esp_bt_gap_get_bond_device_num();
    if (count <= 0) return 0;
    esp_bd_addr_t list[8];
    if (count > 8) count = 8;
    if (esp_bt_gap_get_bond_device_list(&count, list) != ESP_OK) return 0;
    int removed = 0;
    for (int index = 0; index < count; ++index) {
        if (keep != nullptr && std::memcmp(list[index], keep, sizeof(esp_bd_addr_t)) == 0) continue;
        if (esp_bt_gap_remove_bond_device(list[index]) == ESP_OK) ++removed;
    }
    return removed;
}

void push_key(uint8_t usage, uint8_t modifiers) {
    // Call with `lock` held. When full, the newest press is dropped: the
    // consumer has stalled and older keys matter more to what was typed.
    if (key_count >= kKeyQueueLength) return;
    keys[(key_head + key_count) % kKeyQueueLength] = {usage, modifiers};
    ++key_count;
}

// Boot-protocol keyboard report: modifiers, reserved, six key codes.
void handle_keyboard_report(const uint8_t *data, size_t length) {
    if (length < 8) return;
    const uint8_t modifiers = data[0];
    const int64_t now = esp_timer_get_time();
    taskENTER_CRITICAL(&lock);
    held_modifiers = modifiers;
    // 0x01 in every slot is rollover: too many keys down to report which.
    if (data[2] != 0x01) {
        bool held_still_down = false;
        for (size_t index = 2; index < 8; ++index) {
            const uint8_t code = data[index];
            if (code < 0x04) continue;
            if (code == held_usage) held_still_down = true;
            bool already_down = false;
            for (uint8_t previous : previous_keys) already_down = already_down || previous == code;
            if (already_down) continue;
            push_key(code, modifiers);
            // The newest key is the one that repeats. Caps Lock never does.
            if (code != 0x39) {
                held_usage = code;
                held_still_down = true;
                next_repeat_us = now + kRepeatDelayUs;
            }
        }
        if (!held_still_down) held_usage = 0;
        std::memcpy(previous_keys, data + 2, sizeof(previous_keys));
    }
    taskEXIT_CRITICAL(&lock);
}

void handle_open(esp_hidh_dev_t *dev, esp_err_t status) {
    if (status != ESP_OK || dev == nullptr) {
        taskENTER_CRITICAL(&lock);
        if (pairing != CYD_BT_PAIRING_IDLE && pairing != CYD_BT_PAIRING_DONE) pairing = CYD_BT_PAIRING_FAILED;
        changed();
        taskEXIT_CRITICAL(&lock);
        log_event("bt.open failed %d", static_cast<int>(status));
        return;
    }
    const uint8_t *bda = esp_hidh_dev_bda_get(dev);
    const char *device_name = esp_hidh_dev_name_get(dev);
    char name[32] = {};
    esp_hidh_dev_t *previous = nullptr;
    bool new_keyboard = false;
    taskENTER_CRITICAL(&lock);
    // The HID link can open even though authentication just failed; it closes
    // again at once. Neither connect to nor remember such a device.
    if (pairing == CYD_BT_PAIRING_FAILED) {
        taskEXIT_CRITICAL(&lock);
        esp_hidh_dev_close(dev);
        log_event("bt.open refused: auth failed");
        return;
    }
    if (!pairing_requested_for(bda) && !is_remembered(bda)) {
        taskEXIT_CRITICAL(&lock);
        esp_bd_addr_t address;
        std::memcpy(address, bda, sizeof(address));
        esp_hidh_dev_close(dev);
        esp_bt_gap_remove_bond_device(address);
        log_event("bt.open refused: not paired");
        return;
    }
    if (std::memcmp(bda, target_bda, sizeof(esp_bd_addr_t)) == 0 && !unnamed(target_name)) {
        std::memcpy(name, target_name, sizeof(name));
    } else if (remembered && std::memcmp(bda, remembered_bda, sizeof(esp_bd_addr_t)) == 0 &&
               !unnamed(remembered_name)) {
        std::memcpy(name, remembered_name, sizeof(name));
    } else if (device_name != nullptr && device_name[0] != '\0') {
        std::snprintf(name, sizeof(name), "%s", device_name);
    } else {
        format_address(bda, name, sizeof(name));
    }
    if (connected_dev != nullptr && connected_dev != dev) previous = connected_dev;
    connected_dev = dev;
    std::memcpy(connected_name, name, sizeof(name));
    new_keyboard = !remembered || std::memcmp(bda, remembered_bda, sizeof(esp_bd_addr_t)) != 0 ||
                   std::strcmp(remembered_name, name) != 0;
    remembered = true;
    std::memcpy(remembered_bda, bda, sizeof(esp_bd_addr_t));
    std::memcpy(remembered_name, name, sizeof(name));
    if (pairing != CYD_BT_PAIRING_IDLE) pairing = CYD_BT_PAIRING_DONE;
    std::memset(previous_keys, 0, sizeof(previous_keys));
    held_usage = 0;
    changed();
    taskEXIT_CRITICAL(&lock);
    if (previous != nullptr) esp_hidh_dev_close(previous);
    if (new_keyboard) {
        remove_bonds_except(bda);
        save_remembered(bda, name);
    }
    log_event("bt.open %s", name);
    if (unnamed(name)) {
        esp_bd_addr_t address;
        std::memcpy(address, bda, sizeof(address));
        esp_bt_gap_read_remote_name(address);
    }
}

void hidh_callback(void *, esp_event_base_t, int32_t id, void *event_data) {
    const auto event = static_cast<esp_hidh_event_t>(id);
    auto *param = static_cast<esp_hidh_event_data_t *>(event_data);
    switch (event) {
        case ESP_HIDH_OPEN_EVENT:
            handle_open(param->open.dev, param->open.status);
            break;
        case ESP_HIDH_CLOSE_EVENT: {
            bool was_connected = false;
            taskENTER_CRITICAL(&lock);
            if (param->close.dev == connected_dev) {
                was_connected = connected_dev != nullptr;
                connected_dev = nullptr;
                connected_name[0] = '\0';
                std::memset(previous_keys, 0, sizeof(previous_keys));
                held_usage = 0;
            }
            changed();
            taskEXIT_CRITICAL(&lock);
            log_event(was_connected ? "bt.close st %d" : "bt.close other st %d", static_cast<int>(param->close.status));
            break;
        }
        case ESP_HIDH_INPUT_EVENT:
            if (param->input.usage == ESP_HID_USAGE_KEYBOARD && param->input.dev == connected_dev) {
                handle_keyboard_report(param->input.data, param->input.length);
            }
            break;
        default:
            break;
    }
}

void remember_device(const esp_bt_gap_cb_param_t::disc_res_param &result) {
    uint32_t cod = 0;
    int rssi = 0;
    char name[32] = {};
    for (int index = 0; index < result.num_prop; ++index) {
        const esp_bt_gap_dev_prop_t &prop = result.prop[index];
        if (prop.type == ESP_BT_GAP_DEV_PROP_COD) {
            cod = *static_cast<uint32_t *>(prop.val);
        } else if (prop.type == ESP_BT_GAP_DEV_PROP_RSSI) {
            rssi = *static_cast<int8_t *>(prop.val);
        } else if (prop.type == ESP_BT_GAP_DEV_PROP_BDNAME && name[0] == '\0') {
            std::snprintf(name, sizeof(name), "%.*s", prop.len, static_cast<char *>(prop.val));
        } else if (prop.type == ESP_BT_GAP_DEV_PROP_EIR && name[0] == '\0') {
            uint8_t length = 0;
            uint8_t *eir_name = esp_bt_gap_resolve_eir_data(static_cast<uint8_t *>(prop.val),
                                                             ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &length);
            if (eir_name == nullptr) {
                eir_name = esp_bt_gap_resolve_eir_data(static_cast<uint8_t *>(prop.val),
                                                       ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &length);
            }
            if (eir_name != nullptr) std::snprintf(name, sizeof(name), "%.*s", length, eir_name);
        }
    }
    // Inquiry reports a device several times, not always with every property.
    const bool class_known = cod != 0;
    if (class_known && esp_bt_gap_get_cod_major_dev(cod) != ESP_BT_COD_MAJOR_DEV_PERIPHERAL) return;
    // Minor class bit 4 is keyboard, bit 5 pointing device. Mice alone are not listed.
    const uint32_t minor = esp_bt_gap_get_cod_minor_dev(cod);
    const bool keyboard = (minor & 0x10) != 0;
    const bool pointing_only = (minor & 0x20) != 0 && !keyboard;
    taskENTER_CRITICAL(&lock);
    size_t slot = found_count;
    for (size_t index = 0; index < found_count; ++index) {
        if (std::memcmp(found[index].bda, result.bda, sizeof(esp_bd_addr_t)) == 0) slot = index;
    }
    const bool fresh = slot == found_count;
    if (fresh && (!class_known || pointing_only)) {
        taskEXIT_CRITICAL(&lock);
        return;
    }
    if (slot < CYD_BT_MAX_FOUND) {
        auto &device = found[slot];
        std::memcpy(device.bda, result.bda, sizeof(esp_bd_addr_t));
        if (name[0] != '\0' || fresh) std::memcpy(device.name, name, sizeof(device.name));
        if (fresh) device.name_requested = false;
        if (class_known) device.keyboard = keyboard;
        if (rssi != 0 || fresh) device.rssi = rssi;
        if (fresh) ++found_count;
        changed();
    }
    taskEXIT_CRITICAL(&lock);
}

void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT:
            remember_device(param->disc_res);
            break;
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
            const bool now = param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED;
            taskENTER_CRITICAL(&lock);
            discovering = now;
            changed();
            taskEXIT_CRITICAL(&lock);
            if (!now) {
                log_event("bt.scan done %u", static_cast<unsigned>(found_count));
                request_next_name();
            }
            break;
        }
        case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
            log_event("bt.acl up %02x:%02x st %d", param->acl_conn_cmpl_stat.bda[4], param->acl_conn_cmpl_stat.bda[5],
                      static_cast<int>(param->acl_conn_cmpl_stat.stat));
            break;
        case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
            // HCI reason: 0x05 authentication failure, 0x06 key missing,
            // 0x13 remote closed, 0x16 local host closed.
            log_event("bt.acl down %02x:%02x rsn 0x%02x", param->acl_disconn_cmpl_stat.bda[4],
                      param->acl_disconn_cmpl_stat.bda[5], static_cast<int>(param->acl_disconn_cmpl_stat.reason));
            break;
        case ESP_BT_GAP_READ_REMOTE_NAME_EVT:
            handle_remote_name(param->read_rmt_name.bda, param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS,
                               param->read_rmt_name.rmt_name);
            break;
        case ESP_BT_GAP_PIN_REQ_EVT: {
            // Legacy pairing: the host chooses the PIN and shows it; the user
            // types it on the keyboard, then Enter.
            char digits[8];
            std::snprintf(digits, sizeof(digits), "%04u", static_cast<unsigned>(esp_random() % 10000));
            esp_bt_pin_code_t pin = {};
            std::memcpy(pin, digits, 4);
            taskENTER_CRITICAL(&lock);
            const bool accepted = pairing_requested_for(param->pin_req.bda);
            if (accepted) {
                pairing = CYD_BT_PAIRING_PIN;
                std::memcpy(pairing_code, digits, sizeof(pairing_code));
                changed();
            }
            taskEXIT_CRITICAL(&lock);
            esp_bt_gap_pin_reply(param->pin_req.bda, accepted, accepted ? 4 : 0, pin);
            log_event(accepted ? "bt.pin shown" : "bt.pin refused");
            break;
        }
        case ESP_BT_GAP_KEY_NOTIF_EVT: {
            // Secure simple pairing with a keyboard: type this passkey, then Enter.
            // Not shown for an unrequested device, so nobody can type it.
            taskENTER_CRITICAL(&lock);
            const bool accepted = pairing_requested_for(param->key_notif.bda);
            if (accepted) {
                pairing = CYD_BT_PAIRING_PASSKEY;
                std::snprintf(pairing_code, sizeof(pairing_code), "%06lu",
                              static_cast<unsigned long>(param->key_notif.passkey % 1000000));
                changed();
            }
            taskEXIT_CRITICAL(&lock);
            log_event(accepted ? "bt.passkey shown" : "bt.passkey refused");
            break;
        }
        case ESP_BT_GAP_CFM_REQ_EVT: {
            // Numeric comparison, or "just works" for a device without keys or a
            // display: nothing for the user to check, so only the picked device.
            taskENTER_CRITICAL(&lock);
            const bool accepted = pairing_requested_for(param->cfm_req.bda);
            taskEXIT_CRITICAL(&lock);
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, accepted);
            log_event(accepted ? "bt.confirm" : "bt.confirm refused");
            break;
        }
        case ESP_BT_GAP_AUTH_CMPL_EVT: {
            const bool ok = param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS;
            taskENTER_CRITICAL(&lock);
            const bool expected = pairing_requested_for(param->auth_cmpl.bda) || is_remembered(param->auth_cmpl.bda);
            if (!ok && pairing != CYD_BT_PAIRING_IDLE && pairing != CYD_BT_PAIRING_DONE &&
                same_address(target_bda, param->auth_cmpl.bda)) {
                pairing = CYD_BT_PAIRING_FAILED;
            }
            changed();
            taskEXIT_CRITICAL(&lock);
            if (ok && !expected) {
                // Bonded without asking us (a stack-accepted "just works"): undo it.
                esp_bt_gap_remove_bond_device(param->auth_cmpl.bda);
                log_event("bt.auth refused: not paired");
            } else {
                log_event("bt.auth %s", ok ? "ok" : "failed");
            }
            break;
        }
        default:
            break;
    }
}

bool start_bluetooth() {
    esp_bt_controller_config_t config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    config.mode = ESP_BT_MODE_CLASSIC_BT;
    if (esp_bt_controller_init(&config) != ESP_OK) { start_error = "controller init"; return false; }
    if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) { start_error = "controller enable"; return false; }
    if (esp_bluedroid_init() != ESP_OK) { start_error = "bluedroid init"; return false; }
    if (esp_bluedroid_enable() != ESP_OK) { start_error = "bluedroid enable"; return false; }
    if (esp_bt_gap_register_callback(gap_callback) != ESP_OK) { start_error = "gap callback"; return false; }
    esp_bt_io_cap_t io_capability = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &io_capability, sizeof(io_capability));
    esp_bt_pin_code_t unused_pin = {};
    esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, unused_pin);
    esp_bt_gap_set_device_name("CYD Desktop");
    // Bonded keyboards reconnect on their own when they wake.
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    esp_hidh_config_t hidh{};
    hidh.callback = hidh_callback;
    // Measured unused: 3.4 of 4 KB.
    hidh.event_stack_size = 2560;
    hidh.callback_arg = nullptr;
    if (esp_hidh_init(&hidh) != ESP_OK) { start_error = "hidh init"; return false; }
    return true;
}

#endif  // CYD_BT_SUPPORTED

}  // namespace

// MicroPython's split heap asks this how large a new area may be, then takes
// all of it if it needs to (ports/esp32/gccollect.c). An app that grew the GC
// heap that way left 13 KB of a 48 KB heap, the area never came back, and the
// keyboard could no longer connect: each attempt opened and closed at once.
// While Bluetooth runs, keep a reserve out of the GC heap's reach. An app that
// needs more gets MemoryError instead of breaking the keyboard.
extern "C" size_t __real_gc_get_max_new_split(void);
extern "C" size_t __wrap_gc_get_max_new_split(void) {
    const size_t largest = __real_gc_get_max_new_split();
    if (!started) return largest;
    constexpr size_t kBluetoothHeapReserve = 24 * 1024;
    const size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    if (free_heap <= kBluetoothHeapReserve) return 0;
    const size_t allowed = free_heap - kBluetoothHeapReserve;
    return largest < allowed ? largest : allowed;
}

extern "C" int cyd_desktop_radio_mode(void) {
    if (boot_radio_mode < 0) boot_radio_mode = stored_radio_mode();
    return boot_radio_mode;
}

extern "C" int cyd_desktop_radio_next_mode(void) {
    return stored_radio_mode();
}

extern "C" const char *cyd_desktop_radio_mode_name(int mode) {
    switch (mode) {
        case CYD_RADIO_WIFI: return "wifi";
        case CYD_RADIO_BLUETOOTH: return "bluetooth";
        default: return "off";
    }
}

extern "C" bool cyd_desktop_radio_set_next_mode(int mode) {
    if (mode < CYD_RADIO_OFF || mode > CYD_RADIO_BLUETOOTH) return false;
    cyd_desktop_radio_mode();  // pin this boot's mode before NVS changes
    nvs_handle_t handle;
    if (nvs_open("cyd_desktop", NVS_READWRITE, &handle) != ESP_OK) return false;
    const bool ok = nvs_set_u8(handle, "radio_mode", static_cast<uint8_t>(mode)) == ESP_OK &&
                    nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    if (ok) log_event("radio.next %s", cyd_desktop_radio_mode_name(mode));
    return ok;
}

extern "C" void cyd_desktop_radio_restart(void) {
    log_event("radio.restart");
    // Let the protocol task finish its response and the UI its last frame.
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
}

extern "C" void cyd_desktop_bt_start(void) {
#if CYD_BT_SUPPORTED
    if (cyd_desktop_radio_mode() != CYD_RADIO_BLUETOOTH) {
        // Bluetooth is off for this boot: hand the controller's and Bluedroid's
        // static RAM to the heap. This cannot be undone until the next boot.
        esp_bt_mem_release(ESP_BT_MODE_BTDM);
        return;
    }
    load_remembered();
    started = start_bluetooth();
    log_event(started ? "bt.started" : "bt.start failed %s", start_error);
#endif
}

extern "C" void cyd_desktop_bt_snapshot(cyd_bt_snapshot_t *out) {
    *out = cyd_bt_snapshot_t{};
#if CYD_BT_SUPPORTED
    out->running = started;
    taskENTER_CRITICAL(&lock);
    out->discovering = discovering;
    out->connected = connected_dev != nullptr;
    std::memcpy(out->connected_name, connected_name, sizeof(out->connected_name));
    out->bonded = remembered;
    std::memcpy(out->bonded_name, remembered_name, sizeof(out->bonded_name));
    out->pairing = pairing;
    std::memcpy(out->code, pairing_code, sizeof(out->code));
    std::memcpy(out->target_name, target_name, sizeof(out->target_name));
    out->found_count = static_cast<int>(found_count);
    for (size_t index = 0; index < found_count; ++index) {
        auto &device = out->found[index];
        std::memcpy(device.name, found[index].name, sizeof(device.name));
        format_address(found[index].bda, device.address, sizeof(device.address));
        device.rssi = found[index].rssi;
        device.keyboard = found[index].keyboard;
    }
    taskEXIT_CRITICAL(&lock);
#endif
}

extern "C" bool cyd_desktop_bt_connected(void) {
#if CYD_BT_SUPPORTED
    taskENTER_CRITICAL(&lock);
    const bool value = connected_dev != nullptr;
    taskEXIT_CRITICAL(&lock);
    return value;
#else
    return false;
#endif
}

extern "C" int cyd_desktop_bt_keyboard_state(void) {
#if CYD_BT_SUPPORTED
    if (!started) return 0;
    taskENTER_CRITICAL(&lock);
    const int state = connected_dev != nullptr ? 2 : (remembered ? 1 : 0);
    taskEXIT_CRITICAL(&lock);
    return state;
#else
    return 0;
#endif
}

extern "C" uint32_t cyd_desktop_bt_generation(void) {
#if CYD_BT_SUPPORTED
    taskENTER_CRITICAL(&lock);
    const uint32_t value = generation;
    taskEXIT_CRITICAL(&lock);
    return value;
#else
    return 0;
#endif
}

extern "C" void cyd_desktop_bt_status_json(char *out, size_t capacity) {
    cyd_bt_snapshot_t snapshot;
    cyd_desktop_bt_snapshot(&snapshot);
    char devices[640] = {};
    size_t used = 0;
    for (int index = 0; index < snapshot.found_count; ++index) {
        char name[64];
        cyd::desktop::json::escape(snapshot.found[index].name, name, sizeof(name));
        cyd::desktop::json::items_append(devices, sizeof(devices), used,
            "%s{\"index\":%d,\"address\":\"%s\",\"name\":\"%s\",\"keyboard\":%s,\"rssi\":%d}",
            index == 0 ? "" : ",", index, snapshot.found[index].address, name,
            snapshot.found[index].keyboard ? "true" : "false", snapshot.found[index].rssi);
    }
    char connected[64];
    char bonded[64];
    char target[64];
    cyd::desktop::json::escape(snapshot.connected_name, connected, sizeof(connected));
    cyd::desktop::json::escape(snapshot.bonded_name, bonded, sizeof(bonded));
    cyd::desktop::json::escape(snapshot.target_name, target, sizeof(target));
    std::snprintf(out, capacity,
        "{\"radio_mode\":\"%s\",\"running\":%s,\"start_error\":\"%s\",\"discovering\":%s,"
        "\"connected\":%s,\"connected_name\":\"%s\",\"bonded\":%s,\"bonded_name\":\"%s\","
        "\"pairing\":\"%s\",\"code\":\"%s\",\"target_name\":\"%s\",\"found\":[%s]}",
        cyd_desktop_radio_mode_name(cyd_desktop_radio_mode()), snapshot.running ? "true" : "false", start_error,
        snapshot.discovering ? "true" : "false", snapshot.connected ? "true" : "false", connected,
        snapshot.bonded ? "true" : "false", bonded, pairing_name(snapshot.pairing), snapshot.code, target, devices);
}

// Where the RAM goes: heap by capability and each task's unused stack.
extern "C" void cyd_desktop_bt_memory_json(char *out, size_t capacity) {
    char tasks[900] = {};
    [[maybe_unused]] size_t used = 0;
#if configUSE_TRACE_FACILITY
    TaskStatus_t status[24];
    const UBaseType_t count = uxTaskGetSystemState(status, 24, nullptr);
    for (UBaseType_t index = 0; index < count; ++index) {
        cyd::desktop::json::items_append(tasks, sizeof(tasks), used, "%s{\"name\":\"%s\",\"stack_unused\":%lu}",
            index == 0 ? "" : ",", status[index].pcTaskName,
            static_cast<unsigned long>(status[index].usStackHighWaterMark));
    }
#endif
    std::snprintf(out, capacity,
        "{\"heap_8bit_free\":%lu,\"heap_8bit_largest\":%lu,\"heap_8bit_min\":%lu,"
        "\"heap_32bit_free\":%lu,\"heap_exec_free\":%lu,\"tasks\":[%s]}",
        static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
        static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
        static_cast<unsigned long>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)),
        static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_32BIT)),
        static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_EXEC)), tasks);
}

extern "C" void cyd_desktop_bt_pairing_dismiss(void) {
#if CYD_BT_SUPPORTED
    taskENTER_CRITICAL(&lock);
    if (pairing == CYD_BT_PAIRING_FAILED || pairing == CYD_BT_PAIRING_DONE) {
        pairing = CYD_BT_PAIRING_IDLE;
        changed();
    }
    taskEXIT_CRITICAL(&lock);
#endif
}

extern "C" bool cyd_desktop_bt_scan(int seconds) {
#if CYD_BT_SUPPORTED
    if (!started) return false;
    taskENTER_CRITICAL(&lock);
    found_count = 0;
    if (pairing == CYD_BT_PAIRING_FAILED || pairing == CYD_BT_PAIRING_DONE) pairing = CYD_BT_PAIRING_IDLE;
    changed();
    taskEXIT_CRITICAL(&lock);
    const int units = seconds * 100 / 128;  // inquiry length is in 1.28 s steps
    return esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY,
                                      static_cast<uint8_t>(units < 1 ? 1 : (units > 0x30 ? 0x30 : units)), 0) == ESP_OK;
#else
    (void)seconds;
    return false;
#endif
}

extern "C" bool cyd_desktop_bt_connect(int index) {
#if CYD_BT_SUPPORTED
    if (!started || index < 0) return false;
    esp_bd_addr_t bda;
    taskENTER_CRITICAL(&lock);
    const bool valid = static_cast<size_t>(index) < found_count;
    if (valid) {
        std::memcpy(bda, found[index].bda, sizeof(bda));
        std::memcpy(target_bda, bda, sizeof(bda));
        if (found[index].name[0] != '\0') {
            std::memcpy(target_name, found[index].name, sizeof(target_name));
        } else {
            format_address(bda, target_name, sizeof(target_name));
        }
        pairing = CYD_BT_PAIRING_CONNECTING;
        pairing_code[0] = '\0';
        pairing_deadline_us = esp_timer_get_time() + kPairingWindowUs;
        changed();
    }
    taskEXIT_CRITICAL(&lock);
    if (!valid) return false;
    esp_bt_gap_cancel_discovery();
    // Opening is asynchronous: the PIN or passkey, then success or failure,
    // arrive as pairing states.
    esp_hidh_dev_open(bda, ESP_HID_TRANSPORT_BT, 0);
    return true;
#else
    (void)index;
    return false;
#endif
}

extern "C" int cyd_desktop_bt_forget(void) {
#if CYD_BT_SUPPORTED
    if (!started) return 0;
    taskENTER_CRITICAL(&lock);
    esp_hidh_dev_t *device = connected_dev;
    remembered = false;
    remembered_name[0] = '\0';
    pairing = CYD_BT_PAIRING_IDLE;
    changed();
    taskEXIT_CRITICAL(&lock);
    if (device != nullptr) esp_hidh_dev_close(device);
    save_remembered(nullptr, nullptr);
    const int removed = remove_bonds_except(nullptr);
    log_event("bt.forget %d", removed);
    return removed;
#else
    return 0;
#endif
}

extern "C" bool cyd_desktop_bt_key_next(cyd_bt_key_t *out) {
#if CYD_BT_SUPPORTED
    bool found = false;
    taskENTER_CRITICAL(&lock);
    if (key_count > 0) {
        *out = keys[key_head];
        key_head = (key_head + 1) % kKeyQueueLength;
        --key_count;
        found = true;
    } else if (held_usage != 0 && connected_dev != nullptr) {
        const int64_t now = esp_timer_get_time();
        if (now >= next_repeat_us) {
            *out = {held_usage, held_modifiers};
            // A slow consumer gets one repeat per poll, not a burst to catch up.
            next_repeat_us = now - next_repeat_us > kRepeatIntervalUs ? now + kRepeatIntervalUs
                                                                       : next_repeat_us + kRepeatIntervalUs;
            found = true;
        }
    }
    taskEXIT_CRITICAL(&lock);
    return found;
#else
    (void)out;
    return false;
#endif
}
