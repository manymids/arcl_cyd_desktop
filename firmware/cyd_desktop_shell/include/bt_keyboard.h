#pragma once

// Radio mode and the Bluetooth Classic keyboard host (bt_keyboard.cpp).
//
// bt_keyboard.cpp is built by the cyd_bt_keyboard IDF component, so this is
// the only way the shell reaches it: plain C, no Bluedroid types.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CYD_RADIO_OFF = 0,
    CYD_RADIO_WIFI = 1,
    CYD_RADIO_BLUETOOTH = 2,
};

// The mode of this boot, and the mode stored for the next one.
int cyd_desktop_radio_mode(void);
int cyd_desktop_radio_next_mode(void);
const char *cyd_desktop_radio_mode_name(int mode);
bool cyd_desktop_radio_set_next_mode(int mode);
void cyd_desktop_radio_restart(void);

void cyd_desktop_bt_start(void);

enum {
    CYD_BT_PAIRING_IDLE = 0,
    CYD_BT_PAIRING_CONNECTING = 1,  // opening the HID link to a scan result
    CYD_BT_PAIRING_PIN = 2,         // legacy pairing: type `code` then Enter
    CYD_BT_PAIRING_PASSKEY = 3,     // secure simple pairing: type `code` then Enter
    CYD_BT_PAIRING_DONE = 4,        // connected to the device just paired
    CYD_BT_PAIRING_FAILED = 5,
};

#define CYD_BT_MAX_FOUND 6

typedef struct {
    char name[32];
    char address[18];
    int rssi;
    bool keyboard;  // class of device says keyboard
} cyd_bt_found_t;

typedef struct {
    bool running;       // Bluetooth radio mode and the stack started
    bool discovering;
    bool connected;
    char connected_name[32];
    bool bonded;        // a keyboard is remembered
    char bonded_name[32];
    int pairing;        // CYD_BT_PAIRING_*
    char code[8];       // PIN or passkey to type during pairing
    char target_name[32];
    int found_count;
    cyd_bt_found_t found[CYD_BT_MAX_FOUND];
} cyd_bt_snapshot_t;

void cyd_desktop_bt_snapshot(cyd_bt_snapshot_t *out);
bool cyd_desktop_bt_connected(void);
// 0: no keyboard (or Bluetooth is not running), 1: paired but not connected, 2: connected.
int cyd_desktop_bt_keyboard_state(void);
// Changes whenever anything in the snapshot does, so a view can poll it.
uint32_t cyd_desktop_bt_generation(void);

// Returns a finished (done or failed) pairing to idle.
void cyd_desktop_bt_pairing_dismiss(void);
bool cyd_desktop_bt_scan(int seconds);
bool cyd_desktop_bt_connect(int index);
// Forgets the remembered keyboard and disconnects it. Returns bonds removed.
int cyd_desktop_bt_forget(void);
void cyd_desktop_bt_status_json(char *out, size_t capacity);
void cyd_desktop_bt_memory_json(char *out, size_t capacity);

// A key pressed on the connected keyboard: a USB HID usage and the
// boot-protocol modifier byte at that moment.
typedef struct {
    uint8_t usage;
    uint8_t modifiers;
} cyd_bt_key_t;

// The next key press, oldest first; a held key then repeats (500 ms delay,
// 35 ms interval). Returns false when there is none.
bool cyd_desktop_bt_key_next(cyd_bt_key_t *out);

#ifdef __cplusplus
}
#endif
