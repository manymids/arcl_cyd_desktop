#pragma once

// Tile drawing: the primitives every view paints with.
//
// A primitive either writes straight to the panel or, between
// begin_transition_tile() and a flush, into the RAM band [tile_raster_y,
// tile_raster_y + tile_raster_height). Painters do not need to know which:
// fill_rect() clips to the active tile, so a view can run the same paint
// function once per band and the panel receives each band finished.

#include <algorithm>
#include <cstdint>

#include "screen/display.h"
#include "screen/rect.h"
#include "screen/theme.h"
#include "screen/ui_registry.h"

namespace cyd::desktop::screen {

constexpr Rect kScreenBounds{0, 0, kWidth, kHeight};

extern bool clip_enabled;
extern Rect drawing_clip;
extern bool tile_raster_active;
extern int tile_raster_y;
extern int tile_raster_height;

void set_clip(const Rect &rect);
void clear_clip();

void fill_rect(int x, int y, int w, int h, uint16_t color);

void begin_transition_tile(int y, int height);
// Sends the whole band.
void flush_transition_tile();
// Sends only the part of the band inside `requested`.
void flush_transition_rect(const Rect &requested);

// Paints the full screen band by band. `painter` runs once per band; UI nodes
// are collected on the first band only.
template <typename Painter>
void render_in_transition_tiles(Painter painter) {
    clear_clip();
    for (int y = 0; y < kHeight; y += kTransitionTileRows) {
        const int height = std::min(kTransitionTileRows, kHeight - y);
        begin_transition_tile(y, height);
        ui_nodes_suppressed = y != 0;
        painter();
        ui_nodes_suppressed = false;
        clear_clip();
        flush_transition_tile();
    }
}

void frame(int x, int y, int w, int h, uint16_t color);
void text(const char *value, int x, int y, uint16_t color, int scale = 1);
void line(int x0, int y0, int x1, int y1, uint16_t color, int width = 1);
void fill_circle(int center_x, int center_y, int radius, uint16_t color);
void circle(int center_x, int center_y, int radius, uint16_t color, int width = 1);
void fill_round_rect(int x, int y, int width, int height, int radius, uint16_t color);
void round_rect(int x, int y, int width, int height, int radius, uint16_t border, uint16_t fill);
int text_width(const char *value, int scale = 1);
void centered_text(const char *value, int center_x, int y, uint16_t color, int scale = 1);
void centered_text_fit(const char *value, int center_x, int y, int max_width, uint16_t color);

}  // namespace cyd::desktop::screen
