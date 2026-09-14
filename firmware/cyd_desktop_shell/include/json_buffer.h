#pragma once

// Helpers for assembling JSON Lines responses in fixed-size stack buffers.
//
// These exist because the obvious snprintf idiom is wrong in a way that
// produces invalid JSON rather than a short list:
//
//     const int written = std::snprintf(items + used, sizeof(items) - used, ...);
//     if (written >= sizeof(items) - used) break;   // too late
//     used += written;
//
// snprintf writes as much of the item as fits *before* returning the length it
// wanted, so by the time the caller decides to stop, a partial item is already
// sitting in the buffer. Wrapping that in "...[%s]}}" closes the fragment into
// a line no JSON parser will accept. desktop_shortcuts_list shipped this and
// returned `{"shortcut_id":"mjpplayer","a]}}` for a five-shortcut Home screen.
//
// json_items_append rolls the buffer back to the last complete item, so an
// overflow costs the caller entries but never well-formedness.

#include <cstdarg>
#include <cstddef>
#include <cstdio>

namespace cyd::desktop::json {

// Appends one formatted item to `buffer`. Returns true when the whole item
// fit; on false the buffer is left exactly as it was before the call and the
// caller should stop adding items.
inline bool items_append(char *buffer, size_t capacity, size_t &used, const char *format, ...) {
    if (buffer == nullptr || used >= capacity) return false;
    const size_t remaining = capacity - used;

    va_list arguments;
    va_start(arguments, format);
    const int written = std::vsnprintf(buffer + used, remaining, format, arguments);
    va_end(arguments);

    if (written < 0 || static_cast<size_t>(written) >= remaining) {
        buffer[used] = '\0';  // discard the partial item snprintf already wrote
        return false;
    }
    used += static_cast<size_t>(written);
    return true;
}

// True when a completed snprintf into a response buffer was truncated. The
// caller must send an error instead of the truncated line: the wrapper text
// around an item list can overflow even when every item fit.
inline bool response_truncated(int written, size_t capacity) {
    return written < 0 || static_cast<size_t>(written) >= capacity;
}

// Copies `source` into `destination` as the body of a JSON string. Quotes,
// backslashes, newlines and tabs are escaped; other control characters and
// non-ASCII bytes are dropped. An escape pair is never split at the end of the
// buffer: the old protocol.cpp helper fell through and copied a bare quote when
// there was no room for its backslash, which is invalid JSON.
inline void escape(const char *source, char *destination, size_t capacity) {
    if (destination == nullptr || capacity == 0) return;
    size_t used = 0;
    for (size_t index = 0; source != nullptr && source[index] != '\0' && used + 1 < capacity; ++index) {
        const unsigned char character = static_cast<unsigned char>(source[index]);
        char pair = 0;
        if (character == '"' || character == '\\') pair = static_cast<char>(character);
        else if (character == '\n') pair = 'n';
        else if (character == '\t') pair = 't';
        if (pair != 0) {
            if (used + 2 >= capacity) break;
            destination[used++] = '\\';
            destination[used++] = pair;
        } else if (character >= 0x20 && character < 0x7f) {
            destination[used++] = static_cast<char>(character);
        }
    }
    destination[used] = '\0';
}

}  // namespace cyd::desktop::json
