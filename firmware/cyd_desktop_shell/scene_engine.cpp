// The sprite scene of cyd.sprite_present(); see include/scene_engine.h. The
// transform and blending follow the IS01 / 12HT port's native_sprite.c so a
// frame lands on the same pixels; the difference is that it is drawn band by
// band from a pre-computed description instead of into a frame buffer.

#include "scene_engine.h"
#include "ui_font.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace cyd::desktop::scene {

namespace {

constexpr int kWidth = native::kScreenWidth;
constexpr int kHeight = native::kScreenHeight;

struct Image {
    const uint16_t *pixels;
    uint16_t width;
    uint16_t height;
};

// One sprite of the frame, ready to draw: its screen bounding box and the
// inverse transform as 16.16 steps from screen to source pixels.
struct Item {
    uint8_t slot;
    uint8_t alpha;
    uint8_t blend;
    uint8_t direct;  // a plain copy: no rotation, mirror, scale, alpha or blend
    int16_t left, top, right, bottom;  // inclusive, clipped to the screen
    int16_t origin_x, origin_y;        // direct copies: screen position of source (0, 0)
    int32_t step_x, step_y;            // source change per screen column
    int32_t row_x, row_y;              // source change per screen row
    int64_t source_x, source_y;        // source at (left, top)
};

struct Overlay {
    int16_t x, y, width, height;
    uint16_t color;
    uint16_t shadow;
    uint8_t kind;
    uint8_t scale;
    uint8_t align;
    char text[kOverlayTextLength + 1];
};

struct Frame {
    uint16_t background;
    uint16_t item_count;
    uint8_t overlay_count;
    uint8_t queued_count;
    Item items[kItemCapacity];
    Overlay overlays[kOverlayCapacity];
    Overlay queued[kOverlayCapacity];
};

Image images[kImageCapacity]{};
Frame *frame = nullptr;
// 3D: the polygon list and a Z buffer one band high, allocated by begin_3d().
Poly *poly_list = nullptr;
uint16_t *z_band = nullptr;
int poly_count = 0;
bool z_enabled = true;
constexpr int kBandRows = 16;

uint16_t read_u16(const uint8_t *bytes) { return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8)); }
int16_t read_i16(const uint8_t *bytes) { return static_cast<int16_t>(read_u16(bytes)); }

uint16_t blend(uint16_t source, uint16_t destination, int alpha, int mode) {
    if (alpha <= 0) return destination;
    const int sr = (source >> 11) & 0x1f, sg = (source >> 5) & 0x3f, sb = source & 0x1f;
    const int dr = (destination >> 11) & 0x1f, dg = (destination >> 5) & 0x3f, db = destination & 0x1f;
    int r = sr, g = sg, b = sb;
    switch (mode) {
    case kAdd:
        r = std::min(31, sr + dr);
        g = std::min(63, sg + dg);
        b = std::min(31, sb + db);
        break;
    case kSubtract:
        r = std::max(0, dr - sr);
        g = std::max(0, dg - sg);
        b = std::max(0, db - sb);
        break;
    case kMultiply:
        r = sr * dr / 31;
        g = sg * dg / 63;
        b = sb * db / 31;
        break;
    default:
        if (alpha >= 255) return source;
        break;
    }
    r = (r * alpha + dr * (255 - alpha)) / 255;
    g = (g * alpha + dg * (255 - alpha)) / 255;
    b = (b * alpha + db * (255 - alpha)) / 255;
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

// The limits native_sprite.c enforces, against the loaded images.
bool valid(const uint8_t *row) {
    const unsigned slot = read_u16(row);
    if (slot >= static_cast<unsigned>(kImageCapacity) || images[slot].pixels == nullptr) return false;
    const Image &image = images[slot];
    const unsigned scale_x_raw = read_u16(row + 6), scale_y_raw = read_u16(row + 8);
    const int centre_x = read_i16(row + 12), centre_y = read_i16(row + 14);
    return scale_x_raw >= 1 && scale_x_raw <= 4096 && scale_y_raw >= 1 && scale_y_raw <= 4096 && row[19] <= 3 &&
           read_u16(row + 20) <= 3 && centre_x >= 0 && centre_x <= image.width && centre_y >= 0 &&
           centre_y <= image.height;
}

// Fills in `item` from one valid record.
void build(const uint8_t *row, Item &item) {
    const unsigned slot = read_u16(row);
    const Image &image = images[slot];
    const unsigned scale_x_raw = read_u16(row + 6), scale_y_raw = read_u16(row + 8);
    const int centre_x = read_i16(row + 12), centre_y = read_i16(row + 14);
    const unsigned flags = read_u16(row + 20);

    item = Item{};
    item.slot = static_cast<uint8_t>(slot);
    item.alpha = row[18];
    item.blend = row[19];
    // As in native_sprite.c: x, y place the image's (centre_x, centre_y).
    const int x = read_i16(row + 2) - centre_x;
    const int y = read_i16(row + 4) - centre_y;
    const int angle_raw = read_i16(row + 10);
    const bool flip_x = (flags & 1) != 0, flip_y = (flags & 2) != 0;

    if (angle_raw == 0 && !flip_x && !flip_y && scale_x_raw == 256 && scale_y_raw == 256 &&
        item.alpha == 255 && item.blend == kNormal) {
        item.direct = 1;
        item.origin_x = static_cast<int16_t>(std::max(-32768, std::min(32767, x)));
        item.origin_y = static_cast<int16_t>(std::max(-32768, std::min(32767, y)));
        item.left = static_cast<int16_t>(std::max(0, x));
        item.top = static_cast<int16_t>(std::max(0, y));
        item.right = static_cast<int16_t>(std::min(kWidth - 1, x + image.width - 1));
        item.bottom = static_cast<int16_t>(std::min(kHeight - 1, y + image.height - 1));
        return;
    }

    const float scale_x = scale_x_raw / 256.0f, scale_y = scale_y_raw / 256.0f;
    const float angle = angle_raw * (6.28318530718f / 65536.0f);
    const float cosine = std::cos(angle), sine = std::sin(angle);
    const float m00 = cosine * scale_x * (flip_x ? -1.0f : 1.0f);
    const float m01 = -sine * scale_x * (flip_x ? -1.0f : 1.0f);
    const float m10 = sine * scale_y * (flip_y ? -1.0f : 1.0f);
    const float m11 = cosine * scale_y * (flip_y ? -1.0f : 1.0f);

    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    for (int corner = 0; corner < 4; ++corner) {
        const float px = static_cast<float>(((corner & 1) ? image.width : 0) - centre_x);
        const float py = static_cast<float>(((corner & 2) ? image.height : 0) - centre_y);
        const int cx = static_cast<int>(std::floor(px * m00 + py * m01)) + centre_x + x;
        const int cy = static_cast<int>(std::floor(px * m10 + py * m11)) + centre_y + y;
        if (corner == 0 || cx < min_x) min_x = cx;
        if (corner == 0 || cx > max_x) max_x = cx;
        if (corner == 0 || cy < min_y) min_y = cy;
        if (corner == 0 || cy > max_y) max_y = cy;
    }
    // One pixel of margin for rounding; the source bounds test does the rest.
    min_x = std::max(0, min_x - 1);
    min_y = std::max(0, min_y - 1);
    max_x = std::min(kWidth - 1, max_x + 1);
    max_y = std::min(kHeight - 1, max_y + 1);

    const float determinant = m00 * m11 - m01 * m10;
    if (std::fabs(determinant) < 0.0001f || min_x > max_x || min_y > max_y) {
        item.left = 1;  // empty box: nothing to draw
        item.right = 0;
        return;
    }
    const float unit = 65536.0f;
    item.step_x = static_cast<int32_t>(std::lround(m11 / determinant * unit));
    item.step_y = static_cast<int32_t>(std::lround(-m10 / determinant * unit));
    item.row_x = static_cast<int32_t>(std::lround(-m01 / determinant * unit));
    item.row_y = static_cast<int32_t>(std::lround(m00 / determinant * unit));
    const int64_t px = min_x - x - centre_x, py = min_y - y - centre_y;
    item.source_x = static_cast<int64_t>(centre_x) * 65536 + px * item.step_x + py * item.row_x;
    item.source_y = static_cast<int64_t>(centre_y) * 65536 + px * item.step_y + py * item.row_y;
    item.left = static_cast<int16_t>(min_x);
    item.top = static_cast<int16_t>(min_y);
    item.right = static_cast<int16_t>(max_x);
    item.bottom = static_cast<int16_t>(max_y);
}

void draw_item(const native::TileSurface &surface, const Item &item) {
    const int first = std::max<int>(item.top, surface.origin_y);
    const int last = std::min<int>(item.bottom, surface.origin_y + surface.height - 1);
    if (first > last || item.left > item.right) return;
    const Image &image = images[item.slot];
    if (image.pixels == nullptr) return;

    if (item.direct) {
        for (int y = first; y <= last; ++y) {
            const uint16_t *source = image.pixels + static_cast<std::size_t>(y - item.origin_y) * image.width;
            uint16_t *row = surface.pixels + static_cast<std::size_t>(y - surface.origin_y) * surface.stride;
            for (int x = item.left; x <= item.right; ++x) {
                const uint16_t pixel = source[x - item.origin_x];
                if (pixel != 0) row[x] = native::byte_swap(pixel);
            }
        }
        return;
    }

    const bool composite = item.alpha < 255 || item.blend != kNormal;
    int64_t row_x = item.source_x + static_cast<int64_t>(first - item.top) * item.row_x;
    int64_t row_y = item.source_y + static_cast<int64_t>(first - item.top) * item.row_y;
    for (int y = first; y <= last; ++y, row_x += item.row_x, row_y += item.row_y) {
        uint16_t *row = surface.pixels + static_cast<std::size_t>(y - surface.origin_y) * surface.stride;
        int64_t sx = row_x, sy = row_y;
        for (int x = item.left; x <= item.right; ++x, sx += item.step_x, sy += item.step_y) {
            // Toward zero, like native_sprite.c.
            const int source_x = static_cast<int>((sx + (sx < 0 ? 65535 : 0)) >> 16);
            const int source_y = static_cast<int>((sy + (sy < 0 ? 65535 : 0)) >> 16);
            if (static_cast<unsigned>(source_x) >= image.width || static_cast<unsigned>(source_y) >= image.height)
                continue;
            const uint16_t pixel = image.pixels[static_cast<std::size_t>(source_y) * image.width + source_x];
            if (pixel == 0) continue;
            row[x] = composite ? native::byte_swap(blend(pixel, native::byte_swap(row[x]), item.alpha, item.blend))
                               : native::byte_swap(pixel);
        }
    }
}

// As native_3d.c's n3d_blend: additive scales the source by alpha.
uint16_t blend_3d(uint16_t source, uint16_t destination, int alpha, bool add) {
    if (!add && alpha >= 255) return source;
    if (alpha <= 0) return destination;
    const int sr = (source >> 11) & 0x1f, sg = (source >> 5) & 0x3f, sb = source & 0x1f;
    const int dr = (destination >> 11) & 0x1f, dg = (destination >> 5) & 0x3f, db = destination & 0x1f;
    int r, g, b;
    if (add) {
        r = std::min(31, dr + sr * alpha / 255);
        g = std::min(63, dg + sg * alpha / 255);
        b = std::min(31, db + sb * alpha / 255);
    } else {
        r = (sr * alpha + dr * (255 - alpha)) / 255;
        g = (sg * alpha + dg * (255 - alpha)) / 255;
        b = (sb * alpha + db * (255 - alpha)) / 255;
    }
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

void plot_3d(const native::TileSurface &surface, int x, int y, const Poly &poly) {
    uint16_t &pixel = surface.pixels[static_cast<std::size_t>(y - surface.origin_y) * surface.stride + x];
    const bool add = (poly.flags & kPolyAdd) != 0;
    pixel = native::byte_swap(blend_3d(poly.color, native::byte_swap(pixel), poly.alpha, add));
}

// One span of a filled polygon, x1 open, as native_3d.c's fill_span.
void fill_span_3d(const native::TileSurface &surface, int y, int x0, int x1, int32_t z0, int32_t z1,
                  const Poly &poly) {
    const int span = x1 - x0;
    if (span <= 0) return;
    const int32_t dz = static_cast<int32_t>((static_cast<int64_t>(z1 - z0) << 16) / span);
    uint32_t z = static_cast<uint32_t>(z0) << 16;
    if (x0 < 0) {
        z += static_cast<uint32_t>(dz * (0 - x0));
        x0 = 0;
    }
    if (x1 > kWidth) x1 = kWidth;
    if (x0 >= x1) return;
    const bool test = z_enabled && !(poly.flags & kPolyNoZTest);
    const bool write = test && !(poly.flags & kPolyNoZWrite);
    const bool add = (poly.flags & kPolyAdd) != 0;
    const bool solid = !add && poly.alpha >= 255;
    uint16_t *row = surface.pixels + static_cast<std::size_t>(y - surface.origin_y) * surface.stride;
    uint16_t *depth = z_band + static_cast<std::size_t>(y - surface.origin_y) * kWidth;
    const uint16_t swapped = native::byte_swap(poly.color);
    for (int x = x0; x < x1; ++x, z += static_cast<uint32_t>(dz)) {
        if (test) {
            const uint16_t key = static_cast<uint16_t>(z >> 16);
            if (key <= depth[x]) continue;
            if (write) depth[x] = key;
        }
        row[x] = solid ? swapped
                       : native::byte_swap(blend_3d(poly.color, native::byte_swap(row[x]), poly.alpha, add));
    }
}

// Wire frame edges: clipped to the screen, then Bresenham, no depth test.
void line_3d(const native::TileSurface &surface, int x0, int y0, int x1, int y1, const Poly &poly) {
    const double ax = x0, ay = y0, vx = static_cast<double>(x1) - x0, vy = static_cast<double>(y1) - y0;
    const double p[4] = {-vx, vx, -vy, vy};
    const double q[4] = {ax, kWidth - 1 - ax, ay, kHeight - 1 - ay};
    double t0 = 0, t1 = 1;
    for (int k = 0; k < 4; ++k) {
        if (p[k] == 0) {
            if (q[k] < 0) return;
            continue;
        }
        const double t = q[k] / p[k];
        if (p[k] < 0) t0 = std::max(t0, t);
        else t1 = std::min(t1, t);
        if (t0 > t1) return;
    }
    x0 = static_cast<int>(ax + t0 * vx);
    y0 = static_cast<int>(ay + t0 * vy);
    x1 = static_cast<int>(ax + t1 * vx);
    y1 = static_cast<int>(ay + t1 * vy);
    const int band_top = surface.origin_y, band_bottom = surface.origin_y + surface.height;
    if (std::max(y0, y1) < band_top || std::min(y0, y1) >= band_bottom) return;
    const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        if (y0 >= band_top && y0 < band_bottom && x0 >= 0 && x0 < kWidth) plot_3d(surface, x0, y0, poly);
        if (x0 == x1 && y0 == y1) break;
        const int twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

// A convex polygon's rows inside this band, as native_3d.c's DrawPoly.
void draw_poly(const native::TileSurface &surface, const Poly &poly) {
    if (poly.alpha == 0 || poly.count < 2) return;
    if (poly.flags & kPolyWire) {
        for (int i = 0; i < poly.count; ++i) {
            const int j = (i + 1) % poly.count;
            line_3d(surface, poly.x[i], poly.y[i], poly.x[j], poly.y[j], poly);
        }
        return;
    }
    if (poly.count < 3) return;
    int top = poly.y[0], bottom = poly.y[0];
    for (int i = 1; i < poly.count; ++i) {
        top = std::min<int>(top, poly.y[i]);
        bottom = std::max<int>(bottom, poly.y[i]);
    }
    top = std::max(top, std::max(0, surface.origin_y));
    bottom = std::min(bottom, std::min(kHeight, surface.origin_y + surface.height));
    if (top >= bottom) return;
    struct Edge {
        int32_t y0, y1;
        int64_t x, dx, z, dz;  // at y0, Q16
    } edges[kPolyVertices];
    int edge_count = 0;
    for (int i = 0; i < poly.count; ++i) {
        const int j = (i + 1) % poly.count;
        if (poly.y[i] == poly.y[j]) continue;
        const int a = poly.y[i] < poly.y[j] ? i : j, b = a == i ? j : i;
        if (poly.y[b] <= top || poly.y[a] >= bottom) continue;
        const int64_t height = poly.y[b] - poly.y[a];
        Edge &edge = edges[edge_count++];
        edge.y0 = poly.y[a];
        edge.y1 = poly.y[b];
        edge.dx = (static_cast<int64_t>(poly.x[b] - poly.x[a]) << 16) / height;
        edge.dz = (static_cast<int64_t>(poly.z[b] - poly.z[a]) << 16) / height;
        edge.x = static_cast<int64_t>(poly.x[a]) << 16;
        edge.z = static_cast<int64_t>(poly.z[a]) << 16;
    }
    for (int y = top; y < bottom; ++y) {
        int32_t left = INT32_MAX, right = INT32_MIN, z_left = 0, z_right = 0;
        for (int i = 0; i < edge_count; ++i) {
            const Edge &edge = edges[i];
            if (y < edge.y0 || y >= edge.y1) continue;
            const int32_t x = static_cast<int32_t>((edge.x + edge.dx * (y - edge.y0)) >> 16);
            const int32_t z = static_cast<int32_t>((edge.z + edge.dz * (y - edge.y0)) >> 16);
            if (x < left) {
                left = x;
                z_left = z;
            }
            if (x > right) {
                right = x;
                z_right = z;
            }
        }
        if (left == INT32_MAX) continue;
        fill_span_3d(surface, y, left, right + 1, z_left, z_right, poly);
    }
}

void fill_box(const native::TileSurface &surface, int x, int y, int width, int height, uint16_t color) {
    for (int row = std::max(y, surface.origin_y);
         row < std::min(y + height, surface.origin_y + surface.height); ++row)
        surface.hline(x, row, width, color);
}

void draw_text(const native::TileSurface &surface, const char *text, int x, int y, uint16_t color, int scale) {
    if (y + 7 * scale <= surface.origin_y || y >= surface.origin_y + surface.height) return;
    for (const char *p = text; *p != '\0'; ++p, x += 6 * scale) {
        const unsigned char character = static_cast<unsigned char>(*p);
        const uint8_t *columns = character >= 32 && character <= 126 ? ui::kFont5x7[character - 32]
                                                                     : ui::kFont5x7['?' - 32];
        for (int column = 0; column < 5; ++column) {
            for (int bit = 0; bit < 7; ++bit) {
                if (columns[column] & (1 << bit))
                    fill_box(surface, x + column * scale, y + bit * scale, scale, scale, color);
            }
        }
    }
}

void draw_overlay(const native::TileSurface &surface, const Overlay &item) {
    if (item.kind == 1) {
        fill_box(surface, item.x, item.y, item.width, item.height, item.color);
        return;
    }
    if (item.kind == 2) {
        fill_box(surface, item.x, item.y, item.width, 1, item.color);
        fill_box(surface, item.x, item.y + item.height - 1, item.width, 1, item.color);
        fill_box(surface, item.x, item.y, 1, item.height, item.color);
        fill_box(surface, item.x + item.width - 1, item.y, 1, item.height, item.color);
        return;
    }
    const int width = static_cast<int>(std::strlen(item.text)) * 6 * item.scale;
    int x = item.x;
    if (item.align == 1) x -= width / 2;
    else if (item.align == 2) x -= width;
    if (item.shadow != 0) draw_text(surface, item.text, x + item.scale, item.y + item.scale, item.shadow, item.scale);
    draw_text(surface, item.text, x, item.y, item.color, item.scale);
}

}  // namespace

bool begin() {
    if (frame != nullptr) return true;
    frame = static_cast<Frame *>(std::calloc(1, sizeof(Frame)));
    return frame != nullptr;
}

bool begin_3d() {
    if (frame == nullptr) return false;
    if (poly_list == nullptr) poly_list = static_cast<Poly *>(std::calloc(kPolyCapacity, sizeof(Poly)));
    if (z_band == nullptr) z_band = static_cast<uint16_t *>(std::calloc(kWidth * kBandRows, sizeof(uint16_t)));
    return poly_list != nullptr && z_band != nullptr;
}

Poly *polys() { return poly_list; }

void end() {
    std::free(frame);
    frame = nullptr;
    std::free(poly_list);
    poly_list = nullptr;
    std::free(z_band);
    z_band = nullptr;
    poly_count = 0;
    for (Image &image : images) image = Image{};
}

bool ready() { return frame != nullptr; }

bool set_image(int slot, const uint16_t *pixels, int width, int height) {
    if (slot < 0 || slot >= kImageCapacity) return false;
    if (pixels == nullptr) {
        images[slot] = Image{};
        return true;
    }
    if (width < 1 || width > kMaxImageWidth || height < 1 || height > kMaxImageHeight) return false;
    images[slot] = Image{pixels, static_cast<uint16_t>(width), static_cast<uint16_t>(height)};
    return true;
}

bool image_loaded(int slot) { return slot >= 0 && slot < kImageCapacity && images[slot].pixels != nullptr; }

void overlay_clear() {
    if (frame != nullptr) frame->queued_count = 0;
}

bool overlay_add(int kind, int x, int y, int width, int height, uint16_t color,
                 const char *text, int scale, uint16_t shadow, int align) {
    if (frame == nullptr || frame->queued_count >= kOverlayCapacity) return false;
    Overlay &item = frame->queued[frame->queued_count++];
    item = Overlay{};
    item.kind = static_cast<uint8_t>(std::max(0, std::min(2, kind)));
    item.x = static_cast<int16_t>(std::max(-kWidth, std::min(2 * kWidth, x)));
    item.y = static_cast<int16_t>(std::max(-kHeight, std::min(2 * kHeight, y)));
    item.width = static_cast<int16_t>(std::max(0, std::min(kWidth, width)));
    item.height = static_cast<int16_t>(std::max(0, std::min(kHeight, height)));
    item.color = color;
    item.shadow = shadow;
    item.scale = static_cast<uint8_t>(std::max(1, std::min(6, scale)));
    item.align = static_cast<uint8_t>(std::max(0, std::min(2, align)));
    if (text != nullptr) {
        std::strncpy(item.text, text, kOverlayTextLength);
        item.text[kOverlayTextLength] = '\0';
    }
    return true;
}

bool valid_records(const uint8_t *records, std::size_t size) {
    if (size % kRecordBytes != 0 || size / kRecordBytes > static_cast<std::size_t>(kItemCapacity)) return false;
    for (std::size_t offset = 0; offset < size; offset += kRecordBytes)
        if (!valid(records + offset)) return false;
    return true;
}

int prepare(const uint8_t *records, std::size_t size, uint16_t background, int polygons, bool z_buffer) {
    if (frame == nullptr) return kNotReady;
    if (polygons < 0 || polygons > kPolyCapacity || (polygons > 0 && poly_list == nullptr)) return kNotReady;
    if (size % kRecordBytes != 0 || size / kRecordBytes > static_cast<std::size_t>(kItemCapacity)) return kBadRecords;
    const int count = static_cast<int>(size / kRecordBytes);
    // Check everything first, so a bad record leaves the shown frame alone.
    uint16_t depth[kItemCapacity];
    int order[kItemCapacity];
    for (int index = 0; index < count; ++index) {
        const uint8_t *row = records + static_cast<std::size_t>(index) * kRecordBytes;
        if (!valid(row)) return kBadRecords;
        depth[index] = read_u16(row + 16);
        order[index] = index;
    }
    // Larger depth is further back and drawn first; equal depths keep their order.
    std::stable_sort(order, order + count, [&depth](int a, int b) { return depth[a] > depth[b]; });
    int shown = 0;
    for (int index = 0; index < count; ++index) {
        const uint8_t *row = records + static_cast<std::size_t>(order[index]) * kRecordBytes;
        if (row[18] == 0) continue;  // alpha 0 draws nothing
        build(row, frame->items[shown++]);
    }
    frame->item_count = static_cast<uint16_t>(shown);
    frame->background = background;
    poly_count = polygons;
    z_enabled = z_buffer;
    std::memcpy(frame->overlays, frame->queued, sizeof(Overlay) * frame->queued_count);
    frame->overlay_count = frame->queued_count;
    frame->queued_count = 0;
    return count;
}

void render(const native::TileSurface &surface) {
    if (frame == nullptr) {
        surface.fill(0);
        return;
    }
    surface.fill(frame->background);
    if (poly_count > 0 && z_band != nullptr && surface.height <= kBandRows) {
        std::fill(z_band, z_band + kWidth * kBandRows, static_cast<uint16_t>(0));
        for (int index = 0; index < poly_count; ++index) draw_poly(surface, poly_list[index]);
    }
    for (int index = 0; index < frame->item_count; ++index) draw_item(surface, frame->items[index]);
    for (int index = 0; index < frame->overlay_count; ++index) draw_overlay(surface, frame->overlays[index]);
}

}  // namespace cyd::desktop::scene

namespace cyd::desktop::native {

namespace {

void enter() {}
// Leaving the view drops the frame and the image pointers, like the tile map.
void leave() { scene::end(); }
// The script presents frames itself; the shell only repaints on request.
bool update(uint32_t) { return false; }
void input(const InputEvent &) {}

// Not launchable by id: the first cyd.sprite_present() enters it.
const NativeApp app{
    "scene", "SCENE", 1000,
    enter, leave, update, input, scene::render,
    kAppScriptHosted | kAppSceneSurface | kAppFullScreen, nullptr, 0, LauncherIcon::None, nullptr,
};

}  // namespace

const NativeApp &scene_app() { return app; }

}  // namespace cyd::desktop::native
