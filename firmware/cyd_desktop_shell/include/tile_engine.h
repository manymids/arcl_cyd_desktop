#pragma once

// A scrolling tile map with sprites and text, for MicroPython platformers.
//
// The pixel data stays in the app's own MicroPython buffers: the engine keeps
// pointers to them, never a copy, so a game's graphics cost no heap outside
// the app. cyd.py holds references to those buffers for as long as the scene
// is set, and end() drops the pointers before they may be collected.
//
// Graphics are 16x16 cells at 4 bits per pixel, high nibble first, 128 bytes
// a cell. A tile's pixels index palette 0; tile 0 is empty and shows the
// background (palette 0, colour 0). A sprite pixel of 0 is transparent and the
// others index the palette the sprite chooses.
//
// The app builds the next frame (scroll, sprites, text) as a pending scene;
// commit() makes it the shown one. The present path commits and renders under
// the display lock, so a redraw on the UI task never sees half a frame.

#include <cstddef>
#include <cstdint>

#include "native_app.h"

namespace cyd::desktop::tiles {

constexpr int kCell = 16;
constexpr std::size_t kCellBytes = 128;
constexpr int kPaletteColors = 16;
constexpr int kMaxPalettes = 4;
constexpr int kSpriteCapacity = 64;
constexpr int kTextCapacity = 16;
constexpr int kRectCapacity = 8;
constexpr int kTextLength = 32;
constexpr int kMaxMapHeight = 255;

enum SpriteFlags : uint8_t {
    kFlipX = 1 << 0,
    kFlipY = 1 << 1,
    // Drawn only over pixels where the tile map shows colour 0, so a sprite can
    // pass behind scenery (a sprout rising out of a block, a plant in a pipe).
    kBehind = 1 << 2,
    // Bits 4-5 choose the palette.
};

// Result codes shared with cyd.tile_begin().
enum Result : int {
    kOk = 0,
    kBadPalette = -1,
    kBadTiles = -2,
    kBadSprites = -3,
    kBadMap = -4,
};

// Validates and adopts a scene. The buffers must outlive it (see end()).
int begin(const uint8_t *palette, std::size_t palette_size,
          const uint8_t *tiles, std::size_t tiles_size,
          const uint8_t *sprites, std::size_t sprites_size,
          const uint8_t *map, std::size_t map_size, int map_width, int map_height,
          int origin_y);
// Forgets the scene and its buffers. Safe to call when there is none.
void end();
bool ready();
// Replaces the palettes; the count must match begin()'s.
int set_palette(const uint8_t *palette, std::size_t palette_size);

// Pending frame. The map is scrolled by (scroll_x, scroll_y) pixels.
void frame(int scroll_x, int scroll_y = 0);
bool sprite(int cell, int x, int y, uint8_t flags);
// color indexes palette 0. An empty value removes the text.
bool text(int slot, int x, int y, const char *value, uint8_t color, int scale);
// A filled rectangle over the sprites and under the text, for windows and
// on-screen buttons: palette 0 colours, a 2 px border with cut corners unless
// border < 0. Width or height 0 removes it. Kept across frames, like text.
bool rect(int slot, int x, int y, int width, int height, int fill, int border);
void commit();

// Renders one band of the shown scene.
void render(const native::TileSurface &surface);

}  // namespace cyd::desktop::tiles
