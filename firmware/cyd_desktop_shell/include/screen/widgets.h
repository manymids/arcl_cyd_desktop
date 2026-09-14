#pragma once

// Shell furniture built from the drawing primitives: wallpaper, icons, cards,
// buttons, page headers and the taskbar.

#include <cstdint>

#include "screen/draw.h"

namespace cyd::desktop::screen {

constexpr int kTaskbarY = 212;

enum class UiIcon : uint8_t { Start, Clock, Calendar, Scripts, Settings, Home, Game, Movie, Editor };

void card(int x, int y, int width, int height, int radius, uint16_t fill = kCard);
void wallpaper();
void draw_icon(UiIcon icon, int x, int y, uint16_t color);
void native_button(int x, int y, int width, int height, const char *label, bool danger = false);
void native_page_header(UiIcon icon, const char *title, const char *subtitle);

void taskbar();
void register_taskbar_nodes();
// The Bluetooth keyboard symbol at the right end of the taskbar: green while
// connected, amber while paired but disconnected, absent otherwise. Also
// drawn by taskbar(); paints its own background.
void taskbar_keyboard_status();

}  // namespace cyd::desktop::screen
