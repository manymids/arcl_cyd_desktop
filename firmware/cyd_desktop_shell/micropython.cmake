add_library(usermod_cyd_desktop_shell INTERFACE)

target_sources(usermod_cyd_desktop_shell INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/shell_state.cpp
    ${CMAKE_CURRENT_LIST_DIR}/native_app.cpp
    ${CMAKE_CURRENT_LIST_DIR}/native_registry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/app_error.cpp
    ${CMAKE_CURRENT_LIST_DIR}/raycast_demo.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mjp_native_app.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shell_module.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_display.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_draw.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_widgets.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_ui_registry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_touch.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_jpeg.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_keyboard.cpp
    ${CMAKE_CURRENT_LIST_DIR}/keyboard_layout.cpp
    ${CMAKE_CURRENT_LIST_DIR}/key_input.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_keys.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_home.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_clock.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_scripts.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_settings.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_bluetooth.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_editor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen_app.cpp
    ${CMAKE_CURRENT_LIST_DIR}/protocol.cpp
    ${CMAKE_CURRENT_LIST_DIR}/storage.cpp
    ${CMAKE_CURRENT_LIST_DIR}/package_bridge.cpp
    ${CMAKE_CURRENT_LIST_DIR}/runtime_guard.cpp
    ${CMAKE_CURRENT_LIST_DIR}/radio_guard.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../cyd_desktop_board/board_startup.c
)

target_include_directories(usermod_cyd_desktop_shell INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# bt_keyboard.cpp is not listed here: the board's cyd_bt_keyboard IDF component
# builds it (see firmware/cyd_desktop_board/components/cyd_bt_keyboard).

target_link_libraries(usermod INTERFACE usermod_cyd_desktop_shell)
