# Bluetooth keyboard host.
#
# Compiled as its own IDF component instead of a MicroPython user module:
# linking the bt component into the user module adds its hundreds of include
# directories to MicroPython's qstr scan, whose command line then exceeds the
# shell's argument limit. bt_keyboard.cpp itself stays in cyd_desktop_shell.
idf_component_register(
    SRCS "${CMAKE_CURRENT_LIST_DIR}/../../../cyd_desktop_shell/bt_keyboard.cpp"
    PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../../cyd_desktop_shell/include"
    PRIV_REQUIRES bt esp_hid esp_timer nvs_flash)

# MicroPython grows its GC heap into the ESP-IDF heap and keeps the area for
# the rest of the boot. bt_keyboard.cpp wraps the port's size limit so that,
# while Bluetooth runs, growth leaves enough for the stack to (re)connect.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=gc_get_max_new_split")
# cyd_desktop_shell/radio_guard.cpp refuses to start Wi-Fi outside Wi-Fi radio mode.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_event_handler_instance_register" "-Wl,--wrap=esp_now_init")
