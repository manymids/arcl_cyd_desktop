#include "screen/draw.h"
#include "ui_font.h"

extern "C" {
#include "driver/gpio.h"
}

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace cyd::desktop::screen {

bool clip_enabled = false;
Rect drawing_clip{};
bool tile_raster_active = false;
int tile_raster_y = 0;
int tile_raster_height = 0;

void set_clip(const Rect &rect) {
    drawing_clip = intersect_rect(rect, kScreenBounds);
    clip_enabled = true;
}

void clear_clip() { clip_enabled = false; }

void fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (display == nullptr || w <= 0 || h <= 0) return;
    Rect clipped = intersect_rect({x, y, w, h}, kScreenBounds);
    if (clip_enabled) clipped = intersect_rect(clipped, drawing_clip);
    if (tile_raster_active)
        clipped = intersect_rect(clipped, {0, tile_raster_y, kWidth, tile_raster_height});
    if (rect_empty(clipped)) return;
    x = clipped.x;
    y = clipped.y;
    w = clipped.width;
    h = clipped.height;
    if (tile_raster_active) {
        const uint16_t pixel = swap16(color);
        for (int row = 0; row < h; ++row) {
            uint16_t *destination = line_buffer +
                static_cast<size_t>(y + row - tile_raster_y) * kWidth + x;
            std::fill(destination, destination + w, pixel);
        }
        return;
    }
    const size_t pixels = static_cast<size_t>(w) * static_cast<size_t>(h);
    const size_t prepared = std::min(pixels, kSolidBufferPixels);
    for (size_t i = 0; i < prepared; ++i) line_buffer[i] = swap16(color);
    set_window(x, y, w, h);
    gpio_set_level(static_cast<gpio_num_t>(kDisplayDc), 1);
    for (size_t remaining = pixels; remaining > 0;) {
        const size_t chunk = std::min(remaining, kSolidBufferPixels);
        transmit(line_buffer, chunk * sizeof(uint16_t));
        remaining -= chunk;
    }
}

void begin_transition_tile(int y, int height) {
    tile_raster_y = y;
    tile_raster_height = height;
    tile_raster_active = true;
    std::fill(line_buffer, line_buffer + static_cast<size_t>(kWidth) * height, swap16(kBlack));
}

void flush_transition_tile() {
    tile_raster_active = false;
    set_window(0, tile_raster_y, kWidth, tile_raster_height);
    gpio_set_level(static_cast<gpio_num_t>(kDisplayDc), 1);
    transmit(line_buffer, static_cast<size_t>(kWidth) * tile_raster_height * sizeof(uint16_t));
}

void flush_transition_rect(const Rect &requested) {
    const Rect rect = intersect_rect(requested, {0, tile_raster_y, kWidth, tile_raster_height});
    if (rect_empty(rect)) {
        tile_raster_active = false;
        return;
    }
    // Compact the full-width raster rows in place so one SPI transaction can
    // atomically replace the finished dirty rectangle.
    for (int row = 0; row < rect.height; ++row) {
        std::memmove(line_buffer + static_cast<size_t>(row) * rect.width,
                     line_buffer + static_cast<size_t>(rect.y + row - tile_raster_y) * kWidth + rect.x,
                     static_cast<size_t>(rect.width) * sizeof(uint16_t));
    }
    tile_raster_active = false;
    set_window(rect.x, rect.y, rect.width, rect.height);
    gpio_set_level(static_cast<gpio_num_t>(kDisplayDc), 1);
    transmit(line_buffer, static_cast<size_t>(rect.width) * rect.height * sizeof(uint16_t));
}

void frame(int x, int y, int w, int h, uint16_t color) {
    fill_rect(x, y, w, 1, color);
    fill_rect(x, y + h - 1, w, 1, color);
    fill_rect(x, y, 1, h, color);
    fill_rect(x + w - 1, y, 1, h, color);
}

void text(const char *value, int x, int y, uint16_t color, int scale) {
    if (scale < 1) scale = 1;
    for (const char *p = value; *p; ++p) {
        const unsigned char character = static_cast<unsigned char>(*p);
        const uint8_t *columns = character >= 32 && character <= 126
            ? cyd::desktop::ui::kFont5x7[character - 32]
            : cyd::desktop::ui::kFont5x7['?' - 32];
        for (int row = 0; row < 7; ++row) {
            int run_start = -1;
            for (int column = 0; column <= 5; ++column) {
                const bool lit = column < 5 && (columns[column] & (1 << row)) != 0;
                if (lit && run_start < 0) run_start = column;
                if (!lit && run_start >= 0) {
                    fill_rect(x + run_start * scale, y + row * scale,
                              (column - run_start) * scale, scale, color);
                    run_start = -1;
                }
            }
        }
        x += 6 * scale;
    }
}

void line(int x0, int y0, int x1, int y1, uint16_t color, int width) {
    width = std::max(1, width);
    if (x0 == x1) {
        fill_rect(x0 - width / 2, std::min(y0, y1), width, std::abs(y1 - y0) + 1, color);
        return;
    }
    if (y0 == y1) {
        fill_rect(std::min(x0, x1), y0 - width / 2, std::abs(x1 - x0) + 1, width, color);
        return;
    }
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        fill_rect(x0 - width / 2, y0 - width / 2, width, width, color);
        if (x0 == x1 && y0 == y1) break;
        const int twice_error = 2 * error;
        if (twice_error >= dy) { error += dy; x0 += sx; }
        if (twice_error <= dx) { error += dx; y0 += sy; }
    }
}

void fill_circle(int center_x, int center_y, int radius, uint16_t color) {
    if (radius <= 0) return;
    for (int y = -radius; y <= radius; ++y) {
        const int half_width = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - y * y)));
        fill_rect(center_x - half_width, center_y + y, half_width * 2 + 1, 1, color);
    }
}

void circle(int center_x, int center_y, int radius, uint16_t color, int width) {
    if (radius <= 0) return;
    int x = radius;
    int y = 0;
    int error = 1 - radius;
    while (x >= y) {
        const int points[8][2] = {
            {center_x + x, center_y + y}, {center_x + y, center_y + x},
            {center_x - y, center_y + x}, {center_x - x, center_y + y},
            {center_x - x, center_y - y}, {center_x - y, center_y - x},
            {center_x + y, center_y - x}, {center_x + x, center_y - y},
        };
        for (const auto &point : points)
            fill_rect(point[0] - width / 2, point[1] - width / 2, width, width, color);
        ++y;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            --x;
            error += 2 * (y - x + 1);
        }
    }
}

void fill_round_rect(int x, int y, int width, int height, int radius, uint16_t color) {
    if (width <= 0 || height <= 0) return;
    radius = std::max(0, std::min(radius, std::min(width, height) / 2));
    if (radius == 0) {
        fill_rect(x, y, width, height, color);
        return;
    }
    fill_rect(x + radius, y, width - radius * 2, height, color);
    fill_rect(x, y + radius, width, height - radius * 2, color);
    fill_circle(x + radius, y + radius, radius, color);
    fill_circle(x + width - radius - 1, y + radius, radius, color);
    fill_circle(x + radius, y + height - radius - 1, radius, color);
    fill_circle(x + width - radius - 1, y + height - radius - 1, radius, color);
}

void round_rect(int x, int y, int width, int height, int radius, uint16_t border, uint16_t fill) {
    fill_round_rect(x, y, width, height, radius, border);
    if (width > 2 && height > 2) fill_round_rect(x + 1, y + 1, width - 2, height - 2, std::max(0, radius - 1), fill);
}

int text_width(const char *value, int scale) {
    return value == nullptr ? 0 : static_cast<int>(std::strlen(value)) * 6 * scale;
}

void centered_text(const char *value, int center_x, int y, uint16_t color, int scale) {
    text(value, center_x - text_width(value, scale) / 2, y, color, scale);
}

void centered_text_fit(const char *value, int center_x, int y, int max_width, uint16_t color) {
    char fitted[25] = {};
    const size_t maximum = static_cast<size_t>(std::max(1, max_width / 6));
    const size_t length = std::min(std::strlen(value), std::min(maximum, sizeof(fitted) - 1));
    std::memcpy(fitted, value, length);
    if (std::strlen(value) > length && length >= 3) {
        fitted[length - 3] = '.';
        fitted[length - 2] = '.';
        fitted[length - 1] = '.';
    }
    centered_text(fitted, center_x, y, color);
}

}  // namespace cyd::desktop::screen
