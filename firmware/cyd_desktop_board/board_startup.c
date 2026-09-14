void boardctrl_startup(void);
void cyd_desktop_ui_start(void);
void cyd_desktop_protocol_start(void);
void cyd_desktop_runtime_watchdog_initialize(void);
void cyd_desktop_bt_start(void);

void cyd_desktop_board_startup(void) {
    // Preserve the ESP32 port's NVS and flash-size initialisation, then start
    // the native shell task.  The task does not use the MicroPython VM.
    boardctrl_startup();
    cyd_desktop_runtime_watchdog_initialize();
    cyd_desktop_ui_start();
    cyd_desktop_protocol_start();
    // Starts Bluetooth in Bluetooth radio mode; otherwise frees its memory.
    cyd_desktop_bt_start();
}
