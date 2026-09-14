#define MICROPY_HW_BOARD_NAME "CYD Desktop Sandbox"
#define MICROPY_HW_MCU_NAME "ESP32-D0WD-V3"

// Wi-Fi and the network stack, usable when the radio mode is Wi-Fi.
#define MICROPY_PY_NETWORK (1)
#define MICROPY_PY_BLUETOOTH (0)
// Production transport is the native Desktop JSONL server on UART0.
// MicroPython remains embedded for application execution, but does not own
// the CH340 serial link as an interactive REPL.
#define MICROPY_HW_ENABLE_UART_REPL (0)

#define MICROPY_BOARD_STARTUP cyd_desktop_board_startup
void cyd_desktop_board_startup(void);
