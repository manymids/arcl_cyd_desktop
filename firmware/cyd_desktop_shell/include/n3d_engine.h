#pragma once

// N3D geometry for cyd.n3d_* (API level 12): models, the camera, transform,
// near clipping, flat lighting and fog. A port of the IS01 / 12HT port's
// native_3d.c geometry half, producing screen-space polygons that the sprite
// scene (scene_engine.cpp) rasterizes band by band with a 16-row Z buffer.
//
// Fixed point as in native_3d.c: positions and matrices Q16.16, angles 0..65535
// per turn, focal length Q8.8 pixels. Depth keys are near/z mapped to
// 1..65535, larger is nearer.
//
// Plain C++ so the host tests can transform models.

#include <cstddef>
#include <cstdint>

#include "scene_engine.h"

namespace cyd::desktop::n3d {

constexpr int kModelCapacity = 64;
constexpr int kMaxModelVertices = 1024;
constexpr int kMaxModelFaces = 1024;
constexpr std::size_t kItemBytes = 28;   // "<6H3iHBB"
constexpr std::size_t kCameraBytes = 30;  // "<6i3H"
constexpr int kMaxItems = 160;
constexpr int kMaxPoints = 256;

// Item flags ("<6H3iHBB" field 2), as in the Android port.
enum ItemFlags : uint16_t { kEnable = 0x01, kAdd = 0x04, kWire = 0x08 };

enum Result : int {
    kOk = 0,
    kBadModel = -1,
    kNoMemory = -2,
    kBadSlot = -3,
};

struct Projection {
    int centre_x, centre_y;
    int32_t focal_q8;  // pixels, Q8.8
    int32_t near_q16;  // camera z, Q16.16
};

struct Light {
    int16_t x, y, z;  // camera-space direction, Q1.14
    uint8_t ambient;
};

struct Fog {
    uint16_t color;
    int32_t start_q16, end_q16;  // end <= start turns fog off
};

// The settings n3d_present() uses when the app passes none.
Projection default_projection();
Light default_light();
Fog default_fog();
constexpr uint16_t kDefaultClear = 0x0001;

// Frees every model and restores the defaults (app start and end).
void reset();

// Copies a packed model ("<HHhH" counts, radius, flags; "<hhh" vertices;
// "<BBHhhhHHHH" faces) into slot `slot`.
int load_model(int slot, const uint8_t *data, std::size_t size);
bool model_loaded(int slot);

// Sets the view from a 30-byte "<6i3H" camera (eye xyz, target xyz in Q16.16,
// then extra camera rotations) and writes the 3x4 Q16.16 view matrix to
// matrix_out (12 values) when it is not null.
void set_view(const uint8_t camera[kCameraBytes], int32_t *matrix_out);

void set_projection(const Projection &projection);
void set_light(const Light &light);
void set_fog(const Fog &fog);

// Transforms one 28-byte item through the view into polygons appended at
// out[0..capacity). wire forces wire frames. Returns how many were written;
// a disabled item or an empty slot writes none.
int build_item(const uint8_t *item, bool wire, scene::Poly *out, int capacity);

// Projects "<3i" Q16.16 world points to "<hhH" (sx, sy, depth; depth 0 is
// behind the near plane). Returns how many were visible.
int project_points(const uint8_t *points, int count, uint8_t *out);

}  // namespace cyd::desktop::n3d
