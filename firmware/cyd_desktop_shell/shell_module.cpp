#include <cstring>

extern "C" {
#include "py/runtime.h"
#include "py/stream.h"
#include "extmod/vfs.h"
}

#include "shell_state.h"
#include "screen/keyboard.h"

extern "C" void cyd_desktop_sd_set_boot_error(int error);
extern "C" void cyd_desktop_app_begin(const char *app_id);
extern "C" void cyd_desktop_app_title(const char *value);
extern "C" void cyd_desktop_app_clear(void);
extern "C" bool cyd_desktop_app_text(int x, int y, const char *value);
extern "C" bool cyd_desktop_app_button(const char *id, int x, int y, int width, int height, const char *label);
extern "C" bool cyd_desktop_app_primitive(int type, int x0, int y0, int x1, int y1, int color, int stroke);
extern "C" void cyd_desktop_game_begin(int background_top, int background_bottom, int accent);
extern "C" void cyd_desktop_game_frame(uint32_t frame_number, uint32_t score, int lives, int bombs);
extern "C" bool cyd_desktop_game_sprite(int kind, int x, int y, int color, int size);
extern "C" void cyd_desktop_game_touch(bool *pressed, int *x, int *y);
extern "C" bool cyd_desktop_video_begin(void);
extern "C" int cyd_desktop_video_present(const uint8_t *jpeg, size_t size);
extern "C" int cyd_desktop_jpeg_present(const uint8_t *jpeg, size_t size, int x, int y);
extern "C" int cyd_desktop_jpeg_file(const char *path, int x, int y);
extern "C" int cyd_desktop_jpeg_stream(void *user_data,
                                       int (*read_fn)(void *user_data, uint8_t *dest, size_t requested),
                                       int (*seek_fn)(void *user_data, size_t offset),
                                       int x, int y);
extern "C" void cyd_desktop_console_begin(void);
extern "C" void cyd_desktop_console_line(int index, const char *line_text, int color);
extern "C" void cyd_desktop_console_input(const char *input_text, int cursor_pos, const char *prompt);
extern "C" void cyd_desktop_console_options(bool show_keyboard, bool show_input);
extern "C" bool cyd_desktop_app_key_read(char *out, size_t capacity);
extern "C" bool cyd_desktop_bt_connected(void);
extern "C" void cyd_desktop_runtime_set_unlimited(void);
extern "C" void cyd_desktop_console_keyboard(int mode, int pressed_key);
extern "C" void cyd_desktop_app_log(const char *value);
extern "C" void cyd_desktop_ui_refresh(void);
extern "C" void cyd_desktop_ui_refresh_async(void);
extern "C" bool cyd_desktop_app_stop_requested(void);
extern "C" bool cyd_desktop_app_event(char *event, size_t capacity);
extern "C" bool cyd_desktop_runtime_begin(void);
extern "C" bool cyd_desktop_runtime_keepalive(void);
extern "C" void cyd_desktop_runtime_end(void);
extern "C" void cyd_desktop_runtime_fail(void);

namespace {

const char *mp_string(mp_obj_t value) {
    size_t length = 0;
    return mp_obj_str_get_data(value, &length);
}

mp_obj_t snapshot() {
    const auto state = cyd::desktop::shell_state().snapshot();
    mp_obj_t values[3] = {
        mp_obj_new_int(static_cast<int>(state.view)),
        mp_obj_new_bool(state.start_open),
        mp_obj_new_str(state.foreground_app, std::strlen(state.foreground_app)),
    };
    return mp_obj_new_tuple(3, values);
}
static MP_DEFINE_CONST_FUN_OBJ_0(snapshot_obj, snapshot);

mp_obj_t dispatch(mp_obj_t action) {
    const auto value = static_cast<cyd::desktop::Action>(mp_obj_get_int(action));
    return mp_obj_new_bool(cyd::desktop::shell_state().dispatch(value));
}
static MP_DEFINE_CONST_FUN_OBJ_1(dispatch_obj, dispatch);

mp_obj_t sd_boot_result(mp_obj_t error) {
    cyd_desktop_sd_set_boot_error(mp_obj_get_int(error));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(sd_boot_result_obj, sd_boot_result);

mp_obj_t shortcut_clear() { cyd::desktop::shell_state().shortcut_clear(); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_0(shortcut_clear_obj, shortcut_clear);

mp_obj_t shortcut_set(size_t, const mp_obj_t *args) {
    return mp_obj_new_bool(cyd::desktop::shell_state().shortcut_set(mp_string(args[0]), mp_string(args[1]), mp_string(args[2])));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(shortcut_set_obj, 3, 3, shortcut_set);

mp_obj_t shortcut_remove(mp_obj_t id) { return mp_obj_new_bool(cyd::desktop::shell_state().shortcut_remove(mp_string(id))); }
static MP_DEFINE_CONST_FUN_OBJ_1(shortcut_remove_obj, shortcut_remove);

mp_obj_t shortcut_count() { return mp_obj_new_int(cyd::desktop::shell_state().shortcut_count()); }
static MP_DEFINE_CONST_FUN_OBJ_0(shortcut_count_obj, shortcut_count);

mp_obj_t app_catalog_clear() { cyd::desktop::shell_state().app_catalog_clear(); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_0(app_catalog_clear_obj, app_catalog_clear);

mp_obj_t app_catalog_add(size_t, const mp_obj_t *args) {
    return mp_obj_new_bool(cyd::desktop::shell_state().app_catalog_add(mp_string(args[0]), mp_string(args[1])));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(app_catalog_add_obj, 2, 2, app_catalog_add);

mp_obj_t app_catalog_count() { return mp_obj_new_int(cyd::desktop::shell_state().app_catalog_count()); }
static MP_DEFINE_CONST_FUN_OBJ_0(app_catalog_count_obj, app_catalog_count);

extern "C" bool cyd_desktop_runtime_ping(void);
mp_obj_t runtime_ping() { return mp_obj_new_bool(cyd_desktop_runtime_ping()); }
static MP_DEFINE_CONST_FUN_OBJ_0(runtime_ping_obj, runtime_ping);

extern "C" void cyd_desktop_editor_set_text_ex(const char *text, bool truncated);
extern "C" void cyd_desktop_editor_set_text(const char *text);
mp_obj_t editor_set_text_ex(size_t, const mp_obj_t *args) {
    cyd_desktop_editor_set_text_ex(mp_string(args[0]), mp_obj_is_true(args[1]));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(editor_set_text_ex_obj, 2, 2, editor_set_text_ex);

mp_obj_t editor_set_text(mp_obj_t text) {
    cyd_desktop_editor_set_text(mp_string(text));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(editor_set_text_obj, editor_set_text);

extern "C" void cyd_desktop_editor_set_file_list(const char *list);
mp_obj_t editor_set_file_list(mp_obj_t list) {
    cyd_desktop_editor_set_file_list(mp_string(list));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(editor_set_file_list_obj, editor_set_file_list);

extern "C" mp_obj_t cyd_desktop_package_next_py(void);
static MP_DEFINE_CONST_FUN_OBJ_0(package_next_obj, cyd_desktop_package_next_py);

extern "C" mp_obj_t cyd_desktop_package_result_py(size_t n_args, const mp_obj_t *args);
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(package_result_obj, 2, 2, cyd_desktop_package_result_py);

mp_obj_t app_begin(mp_obj_t app_id) { cyd_desktop_app_begin(mp_string(app_id)); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_1(app_begin_obj, app_begin);

mp_obj_t app_title(mp_obj_t value) { cyd_desktop_app_title(mp_string(value)); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_1(app_title_obj, app_title);

mp_obj_t app_clear() { cyd_desktop_app_clear(); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_0(app_clear_obj, app_clear);

mp_obj_t app_text(size_t, const mp_obj_t *args) {
    return mp_obj_new_bool(cyd_desktop_app_text(mp_obj_get_int(args[0]), mp_obj_get_int(args[1]), mp_string(args[2])));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(app_text_obj, 3, 3, app_text);

mp_obj_t app_button(size_t, const mp_obj_t *args) {
    return mp_obj_new_bool(cyd_desktop_app_button(mp_string(args[0]), mp_obj_get_int(args[1]), mp_obj_get_int(args[2]),
        mp_obj_get_int(args[3]), mp_obj_get_int(args[4]), mp_string(args[5])));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(app_button_obj, 6, 6, app_button);

mp_obj_t app_primitive(size_t, const mp_obj_t *args) {
    return mp_obj_new_bool(cyd_desktop_app_primitive(
        mp_obj_get_int(args[0]), mp_obj_get_int(args[1]), mp_obj_get_int(args[2]),
        mp_obj_get_int(args[3]), mp_obj_get_int(args[4]), mp_obj_get_int(args[5]),
        mp_obj_get_int(args[6])));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(app_primitive_obj, 7, 7, app_primitive);

mp_obj_t game_begin(size_t, const mp_obj_t *args) {
    cyd_desktop_game_begin(mp_obj_get_int(args[0]), mp_obj_get_int(args[1]), mp_obj_get_int(args[2]));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(game_begin_obj, 3, 3, game_begin);

mp_obj_t game_frame(size_t, const mp_obj_t *args) {
    cyd_desktop_game_frame(static_cast<uint32_t>(mp_obj_get_int(args[0])),
        static_cast<uint32_t>(mp_obj_get_int(args[1])), mp_obj_get_int(args[2]), mp_obj_get_int(args[3]));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(game_frame_obj, 4, 4, game_frame);

mp_obj_t game_sprite(size_t, const mp_obj_t *args) {
    return mp_obj_new_bool(cyd_desktop_game_sprite(mp_obj_get_int(args[0]), mp_obj_get_int(args[1]),
        mp_obj_get_int(args[2]), mp_obj_get_int(args[3]), mp_obj_get_int(args[4])));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(game_sprite_obj, 5, 5, game_sprite);

mp_obj_t game_touch() {
    bool pressed = false;
    int x = -1;
    int y = -1;
    cyd_desktop_game_touch(&pressed, &x, &y);
    mp_obj_t values[3] = {mp_obj_new_bool(pressed), mp_obj_new_int(x), mp_obj_new_int(y)};
    return mp_obj_new_tuple(3, values);
}
static MP_DEFINE_CONST_FUN_OBJ_0(game_touch_obj, game_touch);

mp_obj_t video_begin() { return mp_obj_new_bool(cyd_desktop_video_begin()); }
static MP_DEFINE_CONST_FUN_OBJ_0(video_begin_obj, video_begin);

mp_obj_t video_present(mp_obj_t value) {
    mp_buffer_info_t buffer{};
    mp_get_buffer_raise(value, &buffer, MP_BUFFER_READ);
    return mp_obj_new_int(cyd_desktop_video_present(static_cast<const uint8_t *>(buffer.buf), buffer.len));
}
static MP_DEFINE_CONST_FUN_OBJ_1(video_present_obj, video_present);

mp_obj_t jpeg_present(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t buffer{};
    mp_get_buffer_raise(args[0], &buffer, MP_BUFFER_READ);
    const int x = n_args > 1 ? mp_obj_get_int(args[1]) : -1;
    const int y = n_args > 2 ? mp_obj_get_int(args[2]) : -1;
    return mp_obj_new_int(cyd_desktop_jpeg_present(static_cast<const uint8_t *>(buffer.buf), buffer.len, x, y));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(jpeg_present_obj, 1, 3, jpeg_present);

static int mp_stream_read_cb(void *user_data, uint8_t *dest, size_t requested) {
    mp_obj_t stream = static_cast<mp_obj_t>(user_data);
    int errcode = 0;
    mp_uint_t n = mp_stream_rw(stream, dest, requested, &errcode, MP_STREAM_RW_READ);
    if (errcode != 0) return -1;
    return static_cast<int>(n);
}

static int mp_stream_seek_cb(void *user_data, size_t offset) {
    mp_obj_t stream = static_cast<mp_obj_t>(user_data);
    int errcode = 0;
    mp_off_t res = mp_stream_seek(stream, offset, MP_SEEK_CUR, &errcode);
    if (errcode != 0 || res < 0) return -1;
    return static_cast<int>(offset);
}

mp_obj_t jpeg_file(size_t n_args, const mp_obj_t *args) {
    const int x = n_args > 1 ? mp_obj_get_int(args[1]) : -1;
    const int y = n_args > 2 ? mp_obj_get_int(args[2]) : -1;

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t open_args[2] = {
            args[0],
            MP_OBJ_NEW_QSTR(MP_QSTR_rb),
        };
        mp_obj_t file = mp_vfs_open(2, open_args, (mp_map_t *)&mp_const_empty_map);
        int result = cyd_desktop_jpeg_stream(file, mp_stream_read_cb, mp_stream_seek_cb, x, y);
        mp_stream_close(file);
        nlr_pop();
        return mp_obj_new_int(result);
    } else {
        return mp_obj_new_int(-5);
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(jpeg_file_obj, 1, 3, jpeg_file);

mp_obj_t console_begin() {
    cyd_desktop_console_begin();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(console_begin_obj, console_begin);

mp_obj_t console_line(size_t n_args, const mp_obj_t *args) {
    cyd_desktop_console_line(mp_obj_get_int(args[0]), mp_string(args[1]), mp_obj_get_int(args[2]));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(console_line_obj, 3, 3, console_line);

mp_obj_t console_input(size_t n_args, const mp_obj_t *args) {
    cyd_desktop_console_input(mp_string(args[0]), mp_obj_get_int(args[1]),
                              n_args > 2 ? mp_string(args[2]) : nullptr);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(console_input_obj, 2, 3, console_input);

mp_obj_t console_keyboard(size_t n_args, const mp_obj_t *args) {
    cyd_desktop_console_keyboard(mp_obj_get_int(args[0]), mp_obj_get_int(args[1]));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(console_keyboard_obj, 2, 2, console_keyboard);

// The on-screen keyboard's layout, shared with the native editor, so apps do
// not keep their own copy of the key positions and characters.
mp_obj_t console_key_at(mp_obj_t x, mp_obj_t y) {
    return mp_obj_new_int(cyd::desktop::screen::keyboard::key_at(mp_obj_get_int(x), mp_obj_get_int(y)));
}
static MP_DEFINE_CONST_FUN_OBJ_2(console_key_at_obj, console_key_at);

mp_obj_t console_key_char(mp_obj_t index, mp_obj_t mode) {
    const mp_int_t key = mp_obj_get_int(index);
    if (key < 0 || key >= cyd::desktop::screen::keyboard::kKeyCount) {
        mp_raise_ValueError(MP_ERROR_TEXT("key index out of range"));
    }
    // Out-of-range modes read as lower case, as they do for console_keyboard().
    const mp_int_t requested_mode = mp_obj_get_int(mode);
    const uint8_t layout = requested_mode >= 0 && requested_mode <= 2 ? static_cast<uint8_t>(requested_mode) : 0;
    const char character = cyd::desktop::screen::keyboard::key_char(layout, static_cast<int>(key));
    return mp_obj_new_str(&character, 1);
}
static MP_DEFINE_CONST_FUN_OBJ_2(console_key_char_obj, console_key_char);

mp_obj_t app_log(mp_obj_t value) { cyd_desktop_app_log(mp_string(value)); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_1(app_log_obj, app_log);

// Commit the retained app surface before Python starts rebuilding the next
// frame. This is the frame boundary: an async request could let the UI task
// observe clear() followed by only a prefix of the next Canvas command list.
extern "C" void cyd_desktop_app_error_set(const char *app_id, const char *text);
mp_obj_t app_error(mp_obj_t app_id, mp_obj_t text) {
    cyd_desktop_app_error_set(mp_string(app_id), mp_string(text));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(app_error_obj, app_error);

mp_obj_t app_update() { cyd_desktop_ui_refresh(); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_0(app_update_obj, app_update);

mp_obj_t app_stop_requested() { return mp_obj_new_bool(cyd_desktop_app_stop_requested()); }
static MP_DEFINE_CONST_FUN_OBJ_0(app_stop_requested_obj, app_stop_requested);

mp_obj_t app_event() {
    char event[32] = {};
    if (!cyd_desktop_app_event(event, sizeof(event))) return mp_const_none;
    return mp_obj_new_str(event, std::strlen(event));
}
static MP_DEFINE_CONST_FUN_OBJ_0(app_event_obj, app_event);

extern "C" int cyd_desktop_radio_mode(void);
extern "C" const char *cyd_desktop_radio_mode_name(int mode);
mp_obj_t key_read() {
    char name[24];
    if (!cyd_desktop_app_key_read(name, sizeof(name))) return mp_const_none;
    return mp_obj_new_str(name, std::strlen(name));
}
static MP_DEFINE_CONST_FUN_OBJ_0(key_read_obj, key_read);

mp_obj_t keyboard_connected() { return mp_obj_new_bool(cyd_desktop_bt_connected()); }
static MP_DEFINE_CONST_FUN_OBJ_0(keyboard_connected_obj, keyboard_connected);

mp_obj_t runtime_no_limit() { cyd_desktop_runtime_set_unlimited(); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_0(runtime_no_limit_obj, runtime_no_limit);

mp_obj_t ui_refresh_async() { cyd_desktop_ui_refresh_async(); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_0(ui_refresh_async_obj, ui_refresh_async);

mp_obj_t console_options(mp_obj_t show_keyboard, mp_obj_t show_input) {
    cyd_desktop_console_options(mp_obj_is_true(show_keyboard), mp_obj_is_true(show_input));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(console_options_obj, console_options);

mp_obj_t radio_mode() {
    const char *name = cyd_desktop_radio_mode_name(cyd_desktop_radio_mode());
    return mp_obj_new_str(name, std::strlen(name));
}
static MP_DEFINE_CONST_FUN_OBJ_0(radio_mode_obj, radio_mode);

mp_obj_t runtime_begin() { return mp_obj_new_bool(cyd_desktop_runtime_begin()); }
static MP_DEFINE_CONST_FUN_OBJ_0(runtime_begin_obj, runtime_begin);

mp_obj_t runtime_keepalive() { return mp_obj_new_bool(cyd_desktop_runtime_keepalive()); }
static MP_DEFINE_CONST_FUN_OBJ_0(runtime_keepalive_obj, runtime_keepalive);

mp_obj_t runtime_end() {
    cyd_desktop_runtime_end();
    // A Ctrl+C that arrived as the app finished must not land in the package worker.
    MP_STATE_MAIN_THREAD(mp_pending_exception) = MP_OBJ_NULL;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(runtime_end_obj, runtime_end);

mp_obj_t runtime_fail() { cyd_desktop_runtime_fail(); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_0(runtime_fail_obj, runtime_fail);

const mp_rom_map_elem_t globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR__cyd_shell)},
    {MP_ROM_QSTR(MP_QSTR_snapshot), MP_ROM_PTR(&snapshot_obj)},
    {MP_ROM_QSTR(MP_QSTR_dispatch), MP_ROM_PTR(&dispatch_obj)},
    {MP_ROM_QSTR(MP_QSTR_sd_boot_result), MP_ROM_PTR(&sd_boot_result_obj)},
    {MP_ROM_QSTR(MP_QSTR_shortcut_clear), MP_ROM_PTR(&shortcut_clear_obj)},
    {MP_ROM_QSTR(MP_QSTR_shortcut_set), MP_ROM_PTR(&shortcut_set_obj)},
    {MP_ROM_QSTR(MP_QSTR_shortcut_remove), MP_ROM_PTR(&shortcut_remove_obj)},
    {MP_ROM_QSTR(MP_QSTR_shortcut_count), MP_ROM_PTR(&shortcut_count_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_catalog_clear), MP_ROM_PTR(&app_catalog_clear_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_catalog_add), MP_ROM_PTR(&app_catalog_add_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_catalog_count), MP_ROM_PTR(&app_catalog_count_obj)},
    {MP_ROM_QSTR(MP_QSTR_package_next), MP_ROM_PTR(&package_next_obj)},
    {MP_ROM_QSTR(MP_QSTR_package_result), MP_ROM_PTR(&package_result_obj)},
    {MP_ROM_QSTR(MP_QSTR_runtime_ping), MP_ROM_PTR(&runtime_ping_obj)},
    {MP_ROM_QSTR(MP_QSTR_editor_set_text), MP_ROM_PTR(&editor_set_text_obj)},
    {MP_ROM_QSTR(MP_QSTR_editor_set_text_ex), MP_ROM_PTR(&editor_set_text_ex_obj)},
    {MP_ROM_QSTR(MP_QSTR_editor_set_file_list), MP_ROM_PTR(&editor_set_file_list_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_begin), MP_ROM_PTR(&app_begin_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_title), MP_ROM_PTR(&app_title_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_clear), MP_ROM_PTR(&app_clear_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_text), MP_ROM_PTR(&app_text_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_button), MP_ROM_PTR(&app_button_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_primitive), MP_ROM_PTR(&app_primitive_obj)},
    {MP_ROM_QSTR(MP_QSTR_game_begin), MP_ROM_PTR(&game_begin_obj)},
    {MP_ROM_QSTR(MP_QSTR_game_frame), MP_ROM_PTR(&game_frame_obj)},
    {MP_ROM_QSTR(MP_QSTR_game_sprite), MP_ROM_PTR(&game_sprite_obj)},
    {MP_ROM_QSTR(MP_QSTR_game_touch), MP_ROM_PTR(&game_touch_obj)},
    {MP_ROM_QSTR(MP_QSTR_video_begin), MP_ROM_PTR(&video_begin_obj)},
    {MP_ROM_QSTR(MP_QSTR_video_present), MP_ROM_PTR(&video_present_obj)},
    {MP_ROM_QSTR(MP_QSTR_jpeg_present), MP_ROM_PTR(&jpeg_present_obj)},
    {MP_ROM_QSTR(MP_QSTR_jpeg_file), MP_ROM_PTR(&jpeg_file_obj)},
    {MP_ROM_QSTR(MP_QSTR_console_begin), MP_ROM_PTR(&console_begin_obj)},
    {MP_ROM_QSTR(MP_QSTR_console_line), MP_ROM_PTR(&console_line_obj)},
    {MP_ROM_QSTR(MP_QSTR_console_input), MP_ROM_PTR(&console_input_obj)},
    {MP_ROM_QSTR(MP_QSTR_console_keyboard), MP_ROM_PTR(&console_keyboard_obj)},
    {MP_ROM_QSTR(MP_QSTR_console_key_at), MP_ROM_PTR(&console_key_at_obj)},
    {MP_ROM_QSTR(MP_QSTR_console_key_char), MP_ROM_PTR(&console_key_char_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_log), MP_ROM_PTR(&app_log_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_error), MP_ROM_PTR(&app_error_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_update), MP_ROM_PTR(&app_update_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_stop_requested), MP_ROM_PTR(&app_stop_requested_obj)},
    {MP_ROM_QSTR(MP_QSTR_app_event), MP_ROM_PTR(&app_event_obj)},
    {MP_ROM_QSTR(MP_QSTR_radio_mode), MP_ROM_PTR(&radio_mode_obj)},
    {MP_ROM_QSTR(MP_QSTR_key_read), MP_ROM_PTR(&key_read_obj)},
    {MP_ROM_QSTR(MP_QSTR_keyboard_connected), MP_ROM_PTR(&keyboard_connected_obj)},
    {MP_ROM_QSTR(MP_QSTR_runtime_no_limit), MP_ROM_PTR(&runtime_no_limit_obj)},
    {MP_ROM_QSTR(MP_QSTR_ui_refresh_async), MP_ROM_PTR(&ui_refresh_async_obj)},
    {MP_ROM_QSTR(MP_QSTR_console_options), MP_ROM_PTR(&console_options_obj)},
    {MP_ROM_QSTR(MP_QSTR_runtime_begin), MP_ROM_PTR(&runtime_begin_obj)},
    {MP_ROM_QSTR(MP_QSTR_runtime_keepalive), MP_ROM_PTR(&runtime_keepalive_obj)},
    {MP_ROM_QSTR(MP_QSTR_runtime_end), MP_ROM_PTR(&runtime_end_obj)},
    {MP_ROM_QSTR(MP_QSTR_runtime_fail), MP_ROM_PTR(&runtime_fail_obj)},
};
static MP_DEFINE_CONST_DICT(globals, globals_table);

}  // namespace

extern "C" const mp_obj_module_t mp_module__cyd_shell = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&globals,
};

MP_REGISTER_MODULE(MP_QSTR__cyd_shell, mp_module__cyd_shell);
