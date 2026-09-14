#pragma once

// What the protocol can observe of the screen: the UI node tree published
// after each render (desktop_ui_tree), the event log (desktop_logs) and the
// last input event (cyd.event()).

#include <cstddef>

namespace cyd::desktop::screen {

constexpr size_t kInputEventCapacity = 32;

// Set while a painter runs for the second and later tiles of one frame, so a
// view that registers its nodes while painting registers them once.
extern bool ui_nodes_suppressed;

void record_event(const char *value);
void record_input(const char *event);

void ui_nodes_begin();
void ui_nodes_end();
void ui_node(const char *id, const char *role, int x, int y, int width, int height,
             const char *label = "", bool enabled = true);

}  // namespace cyd::desktop::screen
