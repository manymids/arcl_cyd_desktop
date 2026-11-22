#pragma once

// The full-screen scene behind cyd.sprite_present() (API level 10): RGB565
// sprites drawn rotated, scaled, mirrored and blended, back to front, with a
// HUD overlay on top. Same record format as the IS01 / 12HT port's NSPR.
//
// There is no frame buffer. A present sorts and pre-computes the frame's
// sprites once, then the frame is rendered 16 rows at a time into the shell's
// transition tile, so the cost in RAM is the frame description (allocated by
// begin(), freed by end()) and never a 150 KB picture.
//
// Sprite pixels stay in the app's MicroPython buffers: the engine keeps
// pointers, like the tile map. cyd.py holds the buffers until it clears the
// slots, and end() drops every pointer before they may be collected.
//
// Plain C++ so the host tests can render bands.

#include <cstddef>
#include <cstdint>

#include "native_app.h"

namespace cyd::desktop::scene {

constexpr int kImageCapacity = 32;
constexpr int kItemCapacity = 128;
constexpr std::size_t kRecordBytes = 22;
constexpr int kOverlayCapacity = 24;
constexpr int kOverlayTextLength = 40;
constexpr int kMaxImageWidth = 320;
constexpr int kMaxImageHeight = 240;

enum Blend : uint8_t { kNormal = 0, kAdd = 1, kSubtract = 2, kMultiply = 3 };

// Screen-space convex polygons from the N3D geometry (n3d_engine.cpp), drawn
// under the sprites with a Z buffer one band high. Depth keys: larger is nearer.
constexpr int kPolyVertices = 6;
constexpr int kPolyCapacity = 320;
enum PolyFlags : uint8_t {
    kPolyAdd = 0x02,
    kPolyWire = 0x04,
    kPolyNoZTest = 0x10,
    kPolyNoZWrite = 0x20,
};
struct Poly {
    int32_t x[kPolyVertices];
    int32_t y[kPolyVertices];
    uint16_t z[kPolyVertices];
    uint16_t color;  // RGB565
    uint8_t count;   // 3..kPolyVertices
    uint8_t flags;   // PolyFlags
    uint8_t alpha;   // 1..255
};

enum Result : int {
    kOk = 0,
    kNoMemory = -1,
    kBadRecords = -2,
    kNotReady = -3,
};

// Allocates the frame description. True when ready (also when already ready).
bool begin();
// Also allocates the polygon list and the band Z buffer. Needs begin().
bool begin_3d();
// The polygon list n3d fills before prepare(); null before begin_3d().
Poly *polys();
// Frees it and forgets every image. Safe to call when there is none.
void end();
bool ready();

// Image slots. pixels is RGB565 in native byte order, width*height values,
// colour 0 transparent; nullptr clears the slot. False for a bad slot or size.
bool set_image(int slot, const uint16_t *pixels, int width, int height);
bool image_loaded(int slot);

// The overlay queued for the next present; it is then shown with that frame
// and the queue starts empty again. kind 0 is text, 1 a filled and 2 an
// outlined rectangle.
void overlay_clear();
bool overlay_add(int kind, int x, int y, int width, int height, uint16_t color,
                 const char *text, int scale, uint16_t shadow, int align);

// Adopts a frame of kRecordBytes "<HhhHHHhhHBBH" records (image id, x, y,
// scale_x, scale_y, angle, centre_x, centre_y, depth, alpha, blend, flags) on
// a background colour, and the queued overlay. Returns the record count, or
// kBadRecords / kNotReady with the shown frame unchanged.
int prepare(const uint8_t *records, std::size_t size, uint16_t background,
            int poly_count = 0, bool z_buffer = true);
// Whether prepare() would accept these records.
bool valid_records(const uint8_t *records, std::size_t size);

// Renders one band of the shown frame.
void render(const native::TileSurface &surface);

}  // namespace cyd::desktop::scene
