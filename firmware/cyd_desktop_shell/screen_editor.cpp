#include "screen/views.h"
#include "screen/keyboard.h"
#include "screen/ui_registry.h"
#include "screen/widgets.h"
#include "shell_state.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

#include <algorithm>
#include <cstdio>
#include <cstring>

extern "C" bool cyd_desktop_package_submit(int operation, const char *app_id, const char *filename,
    const char *payload, uint32_t expected_size, const char *sha256, char *error, size_t error_capacity);
extern "C" bool cyd_desktop_runtime_watchdog_attached(void);
extern "C" bool cyd_desktop_bt_connected(void);

namespace cyd::desktop::screen {

namespace {

char editor_buffer[1024] = {};
int editor_cursor = 0;
int editor_length = 0;
uint8_t editor_kb_mode = 0;
char editor_notice[32] = {};
// Set when the loaded file did not fit editor_buffer. Saving in that state
// would write the truncated copy over the original, so it is refused.
bool editor_truncated = false;
// No default target: an editor that opens pointing at a real config file
// turns a stray SAVE into a write over that file.
char editor_file_path[64] = "";

enum class EditorSubView : uint8_t {
    Text = 0,
    Menu = 1,
    FilePicker = 2,
    SaveAsPrompt = 3,
};
EditorSubView editor_subview = EditorSubView::Text;
char editor_prompt_path[64] = {};
int editor_prompt_cursor = 0;
char editor_files[24][32] = {};
int editor_file_count = 0;
int editor_file_scroll = 0;
// While a hardware keyboard is connected the on-screen keyboard is hidden and
// the text takes its place. Read once per render: every band must agree.
bool editor_hardware_keyboard = false;
constexpr int kRowsWithKeyboard = 7;
constexpr int kRowsWithoutKeyboard = 16;

int editor_visible_rows() {
    return editor_hardware_keyboard ? kRowsWithoutKeyboard : kRowsWithKeyboard;
}

}  // namespace

void editor_new() {
    editor_truncated = false;
    editor_buffer[0] = '\0';
    editor_length = 0;
    editor_cursor = 0;
    std::snprintf(editor_file_path, sizeof(editor_file_path), "/sd/untitled.txt");
    editor_notice[0] = '\0';
    editor_subview = EditorSubView::Text;
}

namespace {

void editor_open_picker() {
    editor_file_count = 0;
    editor_file_scroll = 0;
    for (int retry = 0; retry < 10; ++retry) {
        if (!cyd_desktop_runtime_watchdog_attached()) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    char err[80] = {};
    cyd_desktop_package_submit(11, "/sd", "", "", 0, "", err, sizeof(err));
    editor_subview = EditorSubView::FilePicker;
}

void editor_open_save_as() {
    std::snprintf(editor_prompt_path, sizeof(editor_prompt_path), "%s", editor_file_path);
    editor_prompt_cursor = static_cast<int>(std::strlen(editor_prompt_path));
    editor_subview = EditorSubView::SaveAsPrompt;
}

void editor_prompt_insert(char ch) {
    size_t len = std::strlen(editor_prompt_path);
    if (len >= sizeof(editor_prompt_path) - 2) return;
    std::memmove(editor_prompt_path + editor_prompt_cursor + 1,
                 editor_prompt_path + editor_prompt_cursor,
                 len - editor_prompt_cursor + 1);
    editor_prompt_path[editor_prompt_cursor] = ch;
    ++editor_prompt_cursor;
}

void editor_prompt_backspace() {
    if (editor_prompt_cursor <= 0) return;
    size_t len = std::strlen(editor_prompt_path);
    std::memmove(editor_prompt_path + editor_prompt_cursor - 1,
                 editor_prompt_path + editor_prompt_cursor,
                 len - editor_prompt_cursor + 1);
    --editor_prompt_cursor;
}

void editor_load(const char *path) {
    if (path != nullptr && path[0] != '\0') {
        std::snprintf(editor_file_path, sizeof(editor_file_path), "%s", path);
    }
    editor_notice[0] = '\0';
    for (int retry = 0; retry < 10; ++retry) {
        if (!cyd_desktop_runtime_watchdog_attached()) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    char err[80] = {};
    editor_truncated = false;
    const bool ok = cyd_desktop_package_submit(9, editor_file_path, "", "", 0, "", err, sizeof(err));
    if (!ok) {
        editor_buffer[0] = '\0';
        editor_length = 0;
        editor_cursor = 0;
        editor_truncated = false;
        std::snprintf(editor_notice, sizeof(editor_notice), "OPEN FAILED");
    } else if (editor_truncated) {
        std::snprintf(editor_notice, sizeof(editor_notice), "TOO BIG-READ ONLY");
    }
}

bool editor_save(const char *path) {
    if (editor_truncated) {
        // The buffer holds only the head of the file. Writing it back would
        // delete the rest, which is what this notice exists to prevent.
        std::snprintf(editor_notice, sizeof(editor_notice), "TRUNCATED-NO SAVE");
        return false;
    }
    for (int retry = 0; retry < 10; ++retry) {
        if (!cyd_desktop_runtime_watchdog_attached()) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    char err[80] = {};
    if (!cyd_desktop_package_submit(10, path, "", editor_buffer, 0, "", err, sizeof(err))) {
        std::snprintf(editor_notice, sizeof(editor_notice), "SAVE ERROR!");
        return false;
    }
    std::snprintf(editor_notice, sizeof(editor_notice), "SAVED!");
    return true;
}

void editor_insert(char ch) {
    if (editor_length >= static_cast<int>(sizeof(editor_buffer) - 2)) return;
    std::memmove(editor_buffer + editor_cursor + 1, editor_buffer + editor_cursor, editor_length - editor_cursor + 1);
    editor_buffer[editor_cursor] = ch;
    ++editor_cursor;
    ++editor_length;
    editor_notice[0] = '\0';
}

void editor_backspace() {
    if (editor_cursor <= 0) return;
    std::memmove(editor_buffer + editor_cursor - 1, editor_buffer + editor_cursor, editor_length - editor_cursor + 1);
    --editor_cursor;
    --editor_length;
    editor_notice[0] = '\0';
}

void editor_cursor_move_up() {
    int cur_r = 0, cur_c = 0;
    for (int i = 0; i < editor_cursor; ++i) {
        if (editor_buffer[i] == '\n') {
            ++cur_r;
            cur_c = 0;
        } else {
            ++cur_c;
            if (cur_c >= 48) { ++cur_r; cur_c = 0; }
        }
    }
    if (cur_r == 0) {
        editor_cursor = 0;
        return;
    }
    const int target_r = cur_r - 1;
    const int target_c = cur_c;
    int r = 0, c = 0;
    int best_idx = 0;
    for (int i = 0; i <= editor_length; ++i) {
        if (r == target_r) {
            best_idx = i;
            if (c >= target_c || i == editor_length || editor_buffer[i] == '\n') {
                break;
            }
        } else if (r > target_r) {
            break;
        }
        if (i < editor_length) {
            if (editor_buffer[i] == '\n') {
                ++r;
                c = 0;
            } else {
                ++c;
                if (c >= 48) { ++r; c = 0; }
            }
        }
    }
    editor_cursor = best_idx;
}

void editor_cursor_move_down() {
    int cur_r = 0, cur_c = 0;
    for (int i = 0; i < editor_cursor; ++i) {
        if (editor_buffer[i] == '\n') {
            ++cur_r;
            cur_c = 0;
        } else {
            ++cur_c;
            if (cur_c >= 48) { ++cur_r; cur_c = 0; }
        }
    }
    const int target_r = cur_r + 1;
    const int target_c = cur_c;
    int r = 0, c = 0;
    int best_idx = editor_length;
    bool found_row = false;
    for (int i = 0; i <= editor_length; ++i) {
        if (r == target_r) {
            found_row = true;
            best_idx = i;
            if (c >= target_c || i == editor_length || editor_buffer[i] == '\n') {
                break;
            }
        } else if (r > target_r) {
            break;
        }
        if (i < editor_length) {
            if (editor_buffer[i] == '\n') {
                ++r;
                c = 0;
            } else {
                ++c;
                if (c >= 48) { ++r; c = 0; }
            }
        }
    }
    if (found_row) {
        editor_cursor = best_idx;
    }
}

// The editor wraps at this many columns (screen_editor paints 6 px per character).
constexpr int kEditorColumns = 48;
constexpr int kIndentWidth = 4;

int line_start(int position) {
    while (position > 0 && editor_buffer[position - 1] != '\n') --position;
    return position;
}

int line_end(int position) {
    while (position < editor_length && editor_buffer[position] != '\n') ++position;
    return position;
}

void editor_delete_forward() {
    if (editor_cursor >= editor_length) return;
    std::memmove(editor_buffer + editor_cursor, editor_buffer + editor_cursor + 1, editor_length - editor_cursor);
    --editor_length;
    editor_notice[0] = '\0';
}

// Enter keeps the indentation of the current line, one level deeper after a
// line ending in ':' (Python blocks).
void editor_newline() {
    const int start = line_start(editor_cursor);
    int indent = 0;
    while (start + indent < editor_cursor && editor_buffer[start + indent] == ' ') ++indent;
    int last = editor_cursor - 1;
    while (last >= start && editor_buffer[last] == ' ') --last;
    if (last >= start && editor_buffer[last] == ':') indent += kIndentWidth;
    editor_insert('\n');
    for (int column = 0; column < indent && column < kEditorColumns - 1; ++column) editor_insert(' ');
}

void editor_tab() {
    const int column = editor_cursor - line_start(editor_cursor);
    const int spaces = kIndentWidth - column % kIndentWidth;
    for (int index = 0; index < spaces; ++index) editor_insert(' ');
}

void editor_dedent() {
    const int start = line_start(editor_cursor);
    int spaces = 0;
    while (spaces < kIndentWidth && start + spaces < editor_length && editor_buffer[start + spaces] == ' ') ++spaces;
    if (spaces == 0) return;
    std::memmove(editor_buffer + start, editor_buffer + start + spaces, editor_length - start - spaces + 1);
    editor_length -= spaces;
    editor_cursor = editor_cursor - start >= spaces ? editor_cursor - spaces : start;
    editor_notice[0] = '\0';
}

bool editor_prompt_key(const keys::Key &key) {
    const int length = static_cast<int>(std::strlen(editor_prompt_path));
    if (keys::is_text(key)) {
        editor_prompt_insert(static_cast<char>(key.code));
    } else if (key.code == keys::kBackspace) {
        editor_prompt_backspace();
    } else if (key.code == keys::kLeft) {
        editor_prompt_cursor = std::max(0, editor_prompt_cursor - 1);
    } else if (key.code == keys::kRight) {
        editor_prompt_cursor = std::min(length, editor_prompt_cursor + 1);
    } else if (key.code == keys::kHome) {
        editor_prompt_cursor = 0;
    } else if (key.code == keys::kEnd) {
        editor_prompt_cursor = length;
    } else if (key.code == keys::kEnter) {
        editor_save(editor_prompt_path);
        std::snprintf(editor_file_path, sizeof(editor_file_path), "%s", editor_prompt_path);
        editor_subview = EditorSubView::Text;
        record_input("key.editor.saveas");
    } else if (key.code == keys::kEscape) {
        editor_subview = EditorSubView::Text;
    } else {
        return false;
    }
    return true;
}

bool editor_text_key(const keys::Key &key) {
    if (keys::is_text(key)) {
        editor_insert(static_cast<char>(key.code));
        return true;
    }
    if (keys::is_ctrl(key, 's')) {
        if (editor_file_path[0] == '\0') {
            editor_open_save_as();
        } else {
            editor_save(editor_file_path);
        }
        record_input("key.editor.save");
        return true;
    }
    if (keys::is_ctrl(key, 'o')) {
        editor_open_picker();
        return true;
    }
    const bool ctrl = (key.modifiers & keys::kCtrl) != 0;
    switch (key.code) {
        case keys::kEnter: editor_newline(); break;
        case keys::kTab:
            if (key.modifiers & keys::kShift) editor_dedent();
            else editor_tab();
            break;
        case keys::kBackspace: editor_backspace(); break;
        case keys::kDelete: editor_delete_forward(); break;
        case keys::kLeft: editor_cursor = std::max(0, editor_cursor - 1); break;
        case keys::kRight: editor_cursor = std::min(editor_length, editor_cursor + 1); break;
        case keys::kUp: editor_cursor_move_up(); break;
        case keys::kDown: editor_cursor_move_down(); break;
        case keys::kHome: editor_cursor = ctrl ? 0 : line_start(editor_cursor); break;
        case keys::kEnd: editor_cursor = ctrl ? editor_length : line_end(editor_cursor); break;
        case keys::kPageUp: for (int row = 1; row < editor_visible_rows(); ++row) editor_cursor_move_up(); break;
        case keys::kPageDown: for (int row = 1; row < editor_visible_rows(); ++row) editor_cursor_move_down(); break;
        case keys::kEscape: return false;  // the touch keyboard's ESC reloads the file; a key press should not
        default: return false;
    }
    return true;
}

void paint_editor_menu() {
    fill_rect(0, 0, 320, 240, 0x1082);
    card(25, 10, 270, 198, 6, 0x18e3);
    frame(25, 10, 270, 198, kAccent);
    centered_text("FILE OPERATIONS", 160, 18, kWhite);
    line(35, 32, 285, 32, 0x4208, 1);

    // 1. NEW FILE button
    fill_round_rect(45, 38, 230, 24, 4, kCardAlt);
    centered_text("+ NEW FILE", 160, 45, kBlack);
    ui_node("editor.menu.new", "button", 45, 38, 230, 24, "New File");

    // 2. OPEN FILE button
    fill_round_rect(45, 66, 230, 24, 4, kCardAlt);
    centered_text("OPEN FILE (/sd)...", 160, 73, kBlack);
    ui_node("editor.menu.open", "button", 45, 66, 230, 24, "Open File");

    // 3. SAVE button
    fill_round_rect(45, 94, 230, 24, 4, 0x1405);
    frame(45, 94, 230, 24, kGreen);
    centered_text("SAVE (CURRENT FILE)", 160, 101, kGreen);
    ui_node("editor.menu.save", "button", 45, 94, 230, 24, "Save");

    // 4. SAVE AS button
    fill_round_rect(45, 122, 230, 24, 4, kCardAlt);
    centered_text("SAVE AS...", 160, 129, kBlack);
    ui_node("editor.menu.saveas", "button", 45, 122, 230, 24, "Save As");

    // 5. EXIT button
    fill_round_rect(45, 150, 230, 24, 4, 0x3082);
    frame(45, 150, 230, 24, kRed);
    centered_text("EXIT (CLOSE EDITOR)", 160, 157, 0xFBAA);
    ui_node("editor.menu.exit", "button", 45, 150, 230, 24, "Exit");

    // 6. CANCEL button
    fill_round_rect(45, 178, 230, 24, 4, 0x2945);
    centered_text("CANCEL", 160, 185, kWhite);
    ui_node("editor.menu.cancel", "button", 45, 178, 230, 24, "Cancel");

    taskbar();
}

void paint_editor_file_picker() {
    fill_rect(0, 0, 320, 26, kTaskbarBlue);
    fill_round_rect(3, 3, 54, 20, 3, kCardAlt);
    centered_text("< BACK", 30, 8, kBlack);
    ui_node("editor.picker.back", "button", 3, 3, 54, 20, "< Back");

    centered_text("OPEN FROM /sd", 160, 8, kWhite);

    fill_round_rect(245, 3, 72, 20, 3, 0x2945);
    centered_text("REFRESH", 281, 8, kWhite);
    ui_node("editor.picker.refresh", "button", 245, 3, 72, 20, "Refresh");

    fill_rect(0, 26, 320, 186, 0x1082);

    if (editor_file_count == 0) {
        text("NO FILES FOUND IN /sd", 80, 80, kWhite);
    } else {
        const int visible = std::min(6, editor_file_count - editor_file_scroll);
        for (int i = 0; i < visible; ++i) {
            const int idx = editor_file_scroll + i;
            const int item_y = 30 + i * 29;
            fill_round_rect(10, item_y, 245, 26, 3, 0x2104);
            frame(10, item_y, 245, 26, 0x39E7);

            text("[F]", 16, item_y + 8, kCyan);
            text(editor_files[idx], 40, item_y + 8, kWhite);
            char item_id[32];
            std::snprintf(item_id, sizeof(item_id), "editor.picker.item.%d", i);
            ui_node(item_id, "button", 10, item_y, 245, 26, editor_files[idx]);
        }

        if (editor_file_scroll > 0) {
            fill_round_rect(265, 30, 48, 50, 4, 0x2945);
            frame(265, 30, 48, 50, kAccent);
            centered_text("UP", 289, 48, kWhite);
            ui_node("editor.picker.up", "button", 265, 30, 48, 50, "Up");
        }
        if (editor_file_scroll + 6 < editor_file_count) {
            fill_round_rect(265, 140, 48, 50, 4, 0x2945);
            frame(265, 140, 48, 50, kAccent);
            centered_text("DOWN", 289, 158, kWhite);
            ui_node("editor.picker.down", "button", 265, 140, 48, 50, "Down");
        }
    }

    taskbar();
}

void paint_editor_save_as() {
    fill_rect(0, 0, 320, 26, kTaskbarBlue);
    fill_round_rect(3, 3, 58, 20, 3, kCardAlt);
    centered_text("< CANCEL", 32, 8, kBlack);
    ui_node("editor.saveas.cancel", "button", 3, 3, 58, 20, "< Cancel");

    centered_text("SAVE AS", 150, 8, kWhite);

    fill_round_rect(235, 3, 82, 20, 3, kGreen);
    centered_text("OK / SAVE", 276, 8, kWhite);
    ui_node("editor.saveas.ok", "button", 235, 3, 82, 20, "OK / Save");

    // Prompt card
    card(4, 27, 312, 87, 4, kCard);
    text("ENTER FILE PATH TO SAVE:", 12, 33, kBlack);

    fill_rect(10, 48, 300, 24, kWhite);
    frame(10, 48, 300, 24, kAccent);
    text(editor_prompt_path, 16, 56, kBlack);
    const int cursor_x = 16 + editor_prompt_cursor * 6;
    if (cursor_x < 300) {
        fill_rect(cursor_x, 68, 5, 2, kAccent);
    }

    text("Tap [OK/SAVE] or [ENTER] to save.", 12, 82, 0x4208);

    if (!editor_hardware_keyboard) keyboard::draw(editor_kb_mode, -1);

    taskbar();
}

void paint_editor() {
    if (editor_subview == EditorSubView::Menu) {
        paint_editor_menu();
        return;
    }
    if (editor_subview == EditorSubView::FilePicker) {
        paint_editor_file_picker();
        return;
    }
    if (editor_subview == EditorSubView::SaveAsPrompt) {
        paint_editor_save_as();
        return;
    }

    // Default: EditorSubView::Text
    // Header
    fill_rect(0, 0, 320, 26, kTaskbarBlue);

    // FILE menu button
    fill_round_rect(4, 3, 48, 20, 3, 0x2945);
    centered_text("FILE", 28, 8, kWhite);
    ui_node("editor.menu", "button", 4, 3, 48, 20, "File");

    // Title / file path
    char title_buf[64];
    const char *slash = std::strrchr(editor_file_path, '/');
    const char *display_name = slash ? slash + 1 : editor_file_path;
    std::snprintf(title_buf, sizeof(title_buf), "%s", display_name);
    centered_text_fit(title_buf, 125, 8, 130, kWhite);
    ui_node("editor.title", "label", 56, 3, 138, 20, display_name);

    // 4 cursor buttons: <, ^, v, >
    fill_round_rect(198, 3, 27, 20, 3, kCardAlt);
    centered_text("<", 211, 8, kBlack);
    ui_node("editor.left", "button", 198, 3, 27, 20, "<");

    fill_round_rect(228, 3, 27, 20, 3, kCardAlt);
    centered_text("^", 241, 8, kBlack);
    ui_node("editor.up", "button", 228, 3, 27, 20, "^");

    fill_round_rect(258, 3, 27, 20, 3, kCardAlt);
    centered_text("v", 271, 8, kBlack);
    ui_node("editor.down", "button", 258, 3, 27, 20, "v");

    fill_round_rect(288, 3, 27, 20, 3, kCardAlt);
    centered_text(">", 301, 8, kBlack);
    ui_node("editor.right", "button", 288, 3, 27, 20, ">");

    // Text card: y 27..113 above the on-screen keyboard, 27..210 without it.
    const int rows = editor_visible_rows();
    card(4, 27, 312, editor_hardware_keyboard ? 184 : 87, 4, kCard);
    if (editor_notice[0] != '\0') {
        const bool is_saved = (std::strcmp(editor_notice, "SAVED!") == 0);
        fill_round_rect(100, 29, 120, 16, 3, is_saved ? 0x03E0 : kRed);
        centered_text(editor_notice, 160, 33, kWhite);
    }

    int cursor_row = 0;
    int cur_r = 0, cur_c = 0;
    for (int i = 0; i < editor_length && i < editor_cursor; ++i) {
        if (editor_buffer[i] == '\n') {
            ++cur_r;
            cur_c = 0;
        } else {
            ++cur_c;
            if (cur_c >= 48) { ++cur_r; cur_c = 0; }
        }
    }
    cursor_row = cur_r;
    const int start_row = cursor_row > rows - 1 ? cursor_row - (rows - 1) : 0;

    int draw_r = 0, draw_c = 0;
    int cur_x = 8;
    for (int i = 0; i <= editor_length; ++i) {
        const bool on_screen = (draw_r >= start_row && draw_r < start_row + rows);
        const int screen_y = 31 + (draw_r - start_row) * 11;

        if (i == editor_cursor && on_screen) {
            fill_rect(cur_x, screen_y + 8, 5, 2, kAccent);
        }
        if (i == editor_length) break;

        const char c = editor_buffer[i];
        if (c == '\n') {
            ++draw_r;
            draw_c = 0;
            cur_x = 8;
        } else {
            if (on_screen) {
                char buf[2] = {c, '\0'};
                text(buf, cur_x, screen_y, kBlack);
            }
            cur_x += 6;
            ++draw_c;
            if (draw_c >= 48) {
                ++draw_r;
                draw_c = 0;
                cur_x = 8;
            }
        }
    }

    if (!editor_hardware_keyboard) keyboard::draw(editor_kb_mode, -1);

    taskbar();
}

}  // namespace

bool editor_key(const keys::Key &key) {
    switch (editor_subview) {
        case EditorSubView::Text:
            return editor_text_key(key);
        case EditorSubView::SaveAsPrompt:
            return editor_prompt_key(key);
        case EditorSubView::Menu:
        case EditorSubView::FilePicker:
            if (key.code != keys::kEscape) return false;
            editor_subview = EditorSubView::Text;
            return true;
    }
    return false;
}

void render_editor() {
    editor_hardware_keyboard = cyd_desktop_bt_connected();
    render_in_transition_tiles([]() { paint_editor(); });
}

void handle_editor_touch(int x, int y) {
    if (editor_subview == EditorSubView::Menu) {
        if (x >= 45 && x < 275) {
            // 1. NEW FILE (y=38..62)
            if (y >= 38 && y < 62) {
                editor_new();
                record_input("tap.editor.menu.new");
                render();
                return;
            }
            // 2. OPEN FILE (y=66..90)
            if (y >= 66 && y < 90) {
                editor_open_picker();
                record_input("tap.editor.menu.open");
                render();
                return;
            }
            // 3. SAVE (y=94..118)
            if (y >= 94 && y < 118) {
                editor_save(editor_file_path);
                editor_subview = EditorSubView::Text;
                record_input("tap.editor.menu.save");
                render();
                return;
            }
            // 4. SAVE AS (y=122..146)
            if (y >= 122 && y < 146) {
                editor_open_save_as();
                record_input("tap.editor.menu.saveas");
                render();
                return;
            }
            // 5. EXIT (y=150..174)
            if (y >= 150 && y < 174) {
                editor_subview = EditorSubView::Text;
                cyd::desktop::shell_state().dispatch(cyd::desktop::Action::GoHome);
                record_input("tap.editor.menu.exit");
                render();
                return;
            }
            // 6. CANCEL (y=178..202)
            if (y >= 178 && y < 202) {
                editor_subview = EditorSubView::Text;
                record_input("tap.editor.menu.cancel");
                render();
                return;
            }
        }
        if (x < 25 || x >= 295 || y < 10 || y >= 208) {
            editor_subview = EditorSubView::Text;
            record_input("tap.editor.menu.cancel");
            render();
            return;
        }
        return;
    }

    if (editor_subview == EditorSubView::FilePicker) {
        if (y < 35) {
            if (x < 65) {
                editor_subview = EditorSubView::Text;
                record_input("tap.editor.picker.back");
                render();
                return;
            }
            if (x >= 240) {
                editor_open_picker();
                record_input("tap.editor.picker.refresh");
                render();
                return;
            }
        }
        if (y >= 30 && y < 205) {
            if (x >= 260) {
                if (y < 90 && editor_file_scroll > 0) {
                    editor_file_scroll = std::max(0, editor_file_scroll - 6);
                    record_input("tap.editor.picker.up");
                    render();
                    return;
                }
                if (y >= 135 && editor_file_scroll + 6 < editor_file_count) {
                    editor_file_scroll += 6;
                    record_input("tap.editor.picker.down");
                    render();
                    return;
                }
            } else if (x >= 10 && x < 255) {
                int row = (y - 30) / 29;
                int idx = editor_file_scroll + row;
                if (idx >= 0 && idx < editor_file_count) {
                    char path[64];
                    std::snprintf(path, sizeof(path), "/sd/%s", editor_files[idx]);
                    editor_load(path);
                    editor_subview = EditorSubView::Text;
                    record_input("tap.editor.picker.file");
                    render();
                    return;
                }
            }
        }
        return;
    }

    if (editor_subview == EditorSubView::SaveAsPrompt) {
        if (y < 35) {
            if (x < 65) {
                editor_subview = EditorSubView::Text;
                record_input("tap.editor.saveas.cancel");
                render();
                return;
            }
            if (x >= 235) {
                editor_save(editor_prompt_path);
                std::snprintf(editor_file_path, sizeof(editor_file_path), "%s", editor_prompt_path);
                editor_subview = EditorSubView::Text;
                record_input("tap.editor.saveas.ok");
                render();
                return;
            }
        }
        // No on-screen keys to tap while they are hidden.
        const int k = editor_hardware_keyboard ? -1 : keyboard::key_at(x, y);
        if (k >= 0) {
            char ch = keyboard::key_char(editor_kb_mode, k);
            if (ch == keyboard::kBackspace) {
                editor_prompt_backspace();
                record_input("tap.editor.prompt.bs");
            } else if (ch == keyboard::kCaps) {
                editor_kb_mode = (editor_kb_mode == 1) ? 0 : 1;
                record_input("tap.editor.cap");
            } else if (ch == keyboard::kSymbolShift) {
                editor_kb_mode = (editor_kb_mode == 2) ? 0 : 2;
                record_input("tap.editor.sym");
            } else if (ch == keyboard::kEscape) {
                editor_subview = EditorSubView::Text;
                record_input("tap.editor.saveas.cancel");
            } else if (ch == keyboard::kEnter) {
                editor_save(editor_prompt_path);
                std::snprintf(editor_file_path, sizeof(editor_file_path), "%s", editor_prompt_path);
                editor_subview = EditorSubView::Text;
                record_input("tap.editor.saveas.enter");
            } else {
                editor_prompt_insert(ch);
                record_input("tap.editor.prompt.key");
            }
            render();
            return;
        }
        return;
    }

    // Default: EditorSubView::Text
    if (y < 30) {
        if (x < 55) {
            editor_subview = EditorSubView::Menu;
            record_input("tap.editor.menu");
            render();
            return;
        }
        if (x >= 195 && x < 225) {
            editor_cursor = std::max(0, editor_cursor - 1);
            record_input("tap.editor.left");
            render();
            return;
        }
        if (x >= 225 && x < 255) {
            editor_cursor_move_up();
            record_input("tap.editor.up");
            render();
            return;
        }
        if (x >= 255 && x < 285) {
            editor_cursor_move_down();
            record_input("tap.editor.down");
            render();
            return;
        }
        if (x >= 285) {
            editor_cursor = std::min(editor_length, editor_cursor + 1);
            record_input("tap.editor.right");
            render();
            return;
        }
    }

    // Keyboard (y 117..211), unless hidden for a hardware keyboard.
    const int k = editor_hardware_keyboard ? -1 : keyboard::key_at(x, y);
    if (k >= 0) {
        char ch = keyboard::key_char(editor_kb_mode, k);
        if (ch == keyboard::kBackspace) {
            editor_backspace();
            record_input("tap.editor.bs");
        } else if (ch == keyboard::kCaps) {
            editor_kb_mode = (editor_kb_mode == 1) ? 0 : 1;
            record_input("tap.editor.cap");
        } else if (ch == keyboard::kSymbolShift) {
            editor_kb_mode = (editor_kb_mode == 2) ? 0 : 2;
            record_input("tap.editor.sym");
        } else if (ch == keyboard::kEscape) {
            editor_load(editor_file_path);
            record_input("tap.editor.esc");
        } else {
            editor_insert(ch);
            record_input("tap.editor.key");
        }
        render();
        return;
    }
}

}  // namespace cyd::desktop::screen

using namespace cyd::desktop::screen;

extern "C" void cyd_desktop_editor_set_text_ex(const char *text, bool truncated) {
    editor_truncated = truncated;
    if (text == nullptr) {
        editor_buffer[0] = '\0';
        editor_length = 0;
        editor_cursor = 0;
        return;
    }
    size_t len = std::strlen(text);
    if (len >= sizeof(editor_buffer)) {
        len = sizeof(editor_buffer) - 1;
        editor_truncated = true;
    }
    std::memcpy(editor_buffer, text, len);
    editor_buffer[len] = '\0';
    editor_length = static_cast<int>(len);
    editor_cursor = editor_length;
}

extern "C" void cyd_desktop_editor_set_text(const char *text) {
    cyd_desktop_editor_set_text_ex(text, false);
}

extern "C" bool cyd_desktop_editor_truncated(void) { return editor_truncated; }

extern "C" void cyd_desktop_editor_set_file_list(const char *list) {
    editor_file_count = 0;
    if (list == nullptr || list[0] == '\0') return;
    const char *p = list;
    while (*p != '\0' && editor_file_count < 24) {
        const char *next = std::strchr(p, '\n');
        size_t len = next ? (next - p) : std::strlen(p);
        if (len >= sizeof(editor_files[0])) len = sizeof(editor_files[0]) - 1;
        std::memcpy(editor_files[editor_file_count], p, len);
        editor_files[editor_file_count][len] = '\0';
        ++editor_file_count;
        if (!next) break;
        p = next + 1;
    }
}

