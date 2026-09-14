#pragma once

// RGB565 colours shared by the painters.

#include <cstdint>

namespace cyd::desktop::screen {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kTaskbarBlue = 0x10c4;
constexpr uint16_t kCard = 0xffff;
constexpr uint16_t kCardAlt = 0xeefe;
constexpr uint16_t kDarkGray = 0x63b1;
constexpr uint16_t kShadow = 0x0863;
constexpr uint16_t kYellow = 0xfec0;
constexpr uint16_t kGreen = 0x263b;
constexpr uint16_t kCyan = 0x5e7f;
constexpr uint16_t kRed = 0xf9cb;

// These follow the THEME setting. They keep constant-style names because the
// painters read them as fixed colours; only apply_theme() writes them.
extern uint16_t kWallpaperTop;
extern uint16_t kWallpaperBand;
extern uint16_t kWallpaperMid;
extern uint16_t kWallpaperBottom;
extern uint16_t kWallpaperGlowTop;
extern uint16_t kWallpaperGlowBottom;
extern uint16_t kWindowBlue;
extern uint16_t kAccent;
extern uint16_t kAccentSoft;

// 0 blue, 1 violet, 2 teal; anything else falls back to blue.
void apply_theme(uint8_t theme);

}  // namespace cyd::desktop::screen
