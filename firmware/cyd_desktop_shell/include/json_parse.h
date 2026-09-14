#pragma once

// Minimal JSON Lines request parsing for the desktop protocol.
//
// This is deliberately not a full JSON parser: requests are one flat object per
// line and the firmware has no room for a real one. It does, however, have to
// respect object structure, which the previous strstr-based lookup did not:
//
//     {"id":"n1","command":"desktop_ui_tree","meta":{"limit":1},"limit":4}
//
// strstr found "limit" inside `meta` first, so the request returned one node
// instead of four. find_top_level_value walks the line tracking depth and
// string state, and only matches keys of the outermost object.
//
// String values are safe from key injection either way, because a quote inside
// a value must be escaped and so cannot spell a bare key marker. That was
// confirmed on the device before this header replaced the old lookup.

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace cyd::desktop::json {

// Skips a JSON string that starts at the opening quote. Returns the closing
// quote, or nullptr when the string is unterminated.
inline const char *skip_string(const char *opening_quote) {
    const char *scan = opening_quote + 1;
    while (*scan != '\0' && *scan != '"') {
        if (*scan == '\\' && scan[1] != '\0') ++scan;
        ++scan;
    }
    return (*scan == '"') ? scan : nullptr;
}

// Returns a cursor just past the ':' of `key` in the outermost object of
// `line`, or nullptr when the key is not present at the top level.
inline const char *find_top_level_value(const char *line, const char *key) {
    if (line == nullptr || key == nullptr) return nullptr;
    const char *cursor = line;
    while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
    if (*cursor != '{') return nullptr;
    ++cursor;

    const size_t key_length = std::strlen(key);
    int depth = 0;             // nesting below the outermost object
    bool expecting_key = true;

    while (*cursor != '\0') {
        const char character = *cursor;
        if (std::isspace(static_cast<unsigned char>(character))) { ++cursor; continue; }

        if (character == '"') {
            const char *closing = skip_string(cursor);
            if (closing == nullptr) return nullptr;
            if (depth == 0 && expecting_key &&
                static_cast<size_t>(closing - (cursor + 1)) == key_length &&
                std::strncmp(cursor + 1, key, key_length) == 0) {
                const char *colon = closing + 1;
                while (std::isspace(static_cast<unsigned char>(*colon))) ++colon;
                return (*colon == ':') ? colon + 1 : nullptr;
            }
            expecting_key = false;
            cursor = closing + 1;
            continue;
        }

        if (character == '{' || character == '[') { ++depth; ++cursor; continue; }
        if (character == '}' || character == ']') {
            if (depth == 0) return nullptr;  // end of the outermost object
            --depth;
            expecting_key = false;
            ++cursor;
            continue;
        }
        if (character == ',') { if (depth == 0) expecting_key = true; ++cursor; continue; }
        if (character == ':') { expecting_key = false; ++cursor; continue; }
        ++cursor;
    }
    return nullptr;
}

inline bool has_key(const char *line, const char *key) {
    return find_top_level_value(line, key) != nullptr;
}

// Copies the string value of `key` into `out`. Returns false when the key is
// absent, is not a string, is unterminated, or does not fit `capacity`. A
// false return leaves `out` empty rather than truncated: callers that echo the
// value back (the request id) must not hand the client something it cannot
// match against what it sent.
inline bool string_value(const char *line, const char *key, char *out, size_t capacity) {
    if (out == nullptr || capacity == 0) return false;
    out[0] = '\0';
    const char *cursor = find_top_level_value(line, key);
    if (cursor == nullptr) return false;
    while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
    if (*cursor != '"') return false;
    ++cursor;

    size_t used = 0;
    while (*cursor != '\0' && *cursor != '"') {
        if (used + 1 >= capacity) { out[0] = '\0'; return false; }
        if (*cursor == '\\' && cursor[1] != '\0') ++cursor;
        out[used++] = *cursor++;
    }
    if (*cursor != '"') { out[0] = '\0'; return false; }
    out[used] = '\0';
    return true;
}

inline bool integer_value(const char *line, const char *key, long long *value) {
    if (value == nullptr) return false;
    const char *cursor = find_top_level_value(line, key);
    if (cursor == nullptr) return false;
    while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
    char *end = nullptr;
    const long long parsed = std::strtoll(cursor, &end, 10);
    if (end == cursor) return false;
    *value = parsed;
    return true;
}

}  // namespace cyd::desktop::json
