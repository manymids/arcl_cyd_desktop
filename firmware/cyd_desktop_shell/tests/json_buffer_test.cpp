// Regression test for the desktop_shortcuts_list truncation bug.
//
// On the device, a five-shortcut Home screen produced:
//     {"id":"raw","ok":true,"result":{"shortcuts":[ ...4 items... ,
//      {"shortcut_id":"mjpplayer","a]}}
// which no JSON parser accepts. The buffer had already received a partial
// fifth item by the time the overflow check fired.

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "json_buffer.h"

namespace json = cyd::desktop::json;

namespace {

// The idiom protocol.cpp used at four sites, kept here so the test proves the
// fix addresses a real failure rather than an imagined one.
void append_the_old_way(char *items, size_t capacity, size_t &used, const char *text) {
    const int written = std::snprintf(items + used, capacity - used, "%s", text);
    if (written < 0 || static_cast<size_t>(written) >= capacity - used) return;
    used += static_cast<size_t>(written);
}

bool looks_like_balanced_json(const std::string &line) {
    int depth = 0;
    bool in_string = false;
    for (size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];
        if (in_string) {
            if (character == '\\') ++index;
            else if (character == '"') in_string = false;
            continue;
        }
        if (character == '"') in_string = true;
        else if (character == '{' || character == '[') ++depth;
        else if (character == '}' || character == ']') --depth;
        if (depth < 0) return false;
    }
    return depth == 0 && !in_string;
}

std::string wrap(const char *items) {
    char response[128];
    std::snprintf(response, sizeof(response), "{\"ok\":true,\"result\":{\"items\":[%s]}}", items);
    return response;
}

void the_old_idiom_leaves_a_partial_item() {
    char items[32] = {};
    size_t used = 0;
    append_the_old_way(items, sizeof(items), used, "{\"id\":\"first\"}");
    append_the_old_way(items, sizeof(items), used, ",{\"id\":\"second-and-far-too-long\"}");
    // The overflow check ran, yet the buffer holds more than the first item.
    assert(std::strlen(items) > used);
    assert(!looks_like_balanced_json(wrap(items)));
}

void items_append_rolls_back_to_the_last_complete_item() {
    char items[32] = {};
    size_t used = 0;
    assert(json::items_append(items, sizeof(items), used, "%s", "{\"id\":\"first\"}"));
    const size_t after_first = used;
    assert(!json::items_append(items, sizeof(items), used,
                               "%s", ",{\"id\":\"second-and-far-too-long\"}"));
    assert(used == after_first);
    assert(std::strlen(items) == used);
    assert(std::strcmp(items, "{\"id\":\"first\"}") == 0);
    assert(looks_like_balanced_json(wrap(items)));
}

void a_full_buffer_refuses_further_items() {
    char items[8] = {};
    size_t used = 0;
    assert(json::items_append(items, sizeof(items), used, "%s", "abcdefg"));
    assert(!json::items_append(items, sizeof(items), used, "%s", "h"));
    assert(std::strcmp(items, "abcdefg") == 0);
}

void an_item_that_exactly_fills_the_buffer_is_rejected() {
    // Room for seven characters plus the terminator: an eighth would leave no
    // space for '\0', so snprintf truncates and the item must be dropped.
    char items[8] = {};
    size_t used = 0;
    assert(!json::items_append(items, sizeof(items), used, "%s", "abcdefgh"));
    assert(used == 0);
    assert(items[0] == '\0');
}

void response_truncation_is_detected() {
    char response[16];
    const int written = std::snprintf(response, sizeof(response), "%s", "0123456789abcdefghij");
    assert(json::response_truncated(written, sizeof(response)));

    const int short_write = std::snprintf(response, sizeof(response), "%s", "ok");
    assert(!json::response_truncated(short_write, sizeof(response)));
}

void five_shortcuts_stay_parseable() {
    // The shape that broke on the device: more shortcuts than the buffer holds.
    struct Shortcut { const char *id; const char *app; const char *title; };
    const Shortcut shortcuts[] = {
        {"pyconsole", "console", "CONSOLE"},
        {"sc_weather", "weather", "WEATHER"},
        {"sc_pixel", "pixelstorm", "PIXEL STORM"},
        {"sc_jpeg", "jpegviewer", "JPEG VIEW"},
        {"mjpplayer", "mjpplayer", "MJP PLAYER"},
    };
    char items[300] = {};
    size_t used = 0;
    unsigned returned = 0;
    for (const auto &shortcut : shortcuts) {
        if (!json::items_append(items, sizeof(items), used,
                "%s{\"shortcut_id\":\"%s\",\"app_id\":\"%s\",\"title\":\"%s\"}",
                returned == 0 ? "" : ",", shortcut.id, shortcut.app, shortcut.title)) {
            break;
        }
        ++returned;
    }
    char response[448];
    const int written = std::snprintf(response, sizeof(response),
        "{\"id\":\"t\",\"ok\":true,\"result\":{\"total\":5,\"returned\":%u,\"truncated\":%s,\"shortcuts\":[%s]}}",
        returned, returned < 5 ? "true" : "false", items);
    assert(!json::response_truncated(written, sizeof(response)));
    assert(looks_like_balanced_json(response));
    // Whatever fits, the caller can tell how much was left out.
    assert(returned >= 1 && returned <= 5);
}

void escape_produces_a_valid_json_string_body() {
    char out[64];
    json::escape("say \"hi\"\\now", out, sizeof(out));
    assert(std::strcmp(out, "say \\\"hi\\\"\\\\now") == 0);

    // Tracebacks are multi-line; newlines and tabs must survive as escapes.
    json::escape("line 1\n\tline 2", out, sizeof(out));
    assert(std::strcmp(out, "line 1\\n\\tline 2") == 0);

    // Other control bytes and non-ASCII are dropped rather than emitted raw.
    // Adjacent literals: "\x01c" would otherwise parse as the single escape \x01c.
    json::escape("a\rb\x01" "c\xe3\x81\x82" "d", out, sizeof(out));
    assert(std::strcmp(out, "abcd") == 0);
}

void escape_never_splits_a_pair_at_the_end() {
    // Room for "ab" plus terminator only: the quote's backslash does not fit.
    char out[4];
    json::escape("ab\"cd", out, sizeof(out));
    assert(std::strcmp(out, "ab") == 0);
    assert(looks_like_balanced_json(std::string("{\"k\":\"") + out + "\"}"));

    // The old helper copied a bare quote here, yielding {"k":"ab"}.
    char exact[5];
    json::escape("ab\"", exact, sizeof(exact));
    assert(std::strcmp(exact, "ab\\\"") == 0);

    json::escape(nullptr, out, sizeof(out));
    assert(out[0] == '\0');
}

}  // namespace

int main() {
    the_old_idiom_leaves_a_partial_item();
    items_append_rolls_back_to_the_last_complete_item();
    a_full_buffer_refuses_further_items();
    an_item_that_exactly_fills_the_buffer_is_rejected();
    response_truncation_is_detected();
    five_shortcuts_stay_parseable();
    escape_produces_a_valid_json_string_body();
    escape_never_splits_a_pair_at_the_end();
    std::printf("json_buffer_test: all assertions passed\n");
    return 0;
}
