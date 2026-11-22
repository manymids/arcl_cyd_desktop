#pragma once

// cyd.game_meshes() (API level 11): projects small packed meshes onto the
// game view as flat triangles, back to front. A port of the IS01 / 12HT
// port's is01_mesh.inc, with the same mesh format and camera model.
//
// Mesh: "<HH" vertex and face counts, then "<hhh" per vertex, then "<HB4B"
// per face (RGB565 colour, 3 or 4 corners, corner indexes). Faces whose
// corners are anticlockwise on screen are culled unless the camera draws wire
// frames, and a face with a corner nearer than kNearZ is dropped whole.
//
// Plain C++ so the host tests can project meshes.

#include <cstddef>
#include <cstdint>

namespace cyd::desktop::mesh {

constexpr int kMaxVertices = 256;
constexpr int kMaxFaces = 256;
constexpr int kMaxSceneFaces = 512;
constexpr int kMaxInstances = 32;
constexpr float kNearZ = 14.0f;
constexpr int kCentreX = 160;
constexpr int kCentreY = 112;

struct Camera {
    float x, y, z;           // position
    float yaw, pitch, roll;  // radians
    float focal;             // pixels, > 0
    bool wire;
};

struct Instance {
    const uint8_t *data;
    std::size_t size;
    float x, y, z;
    float yaw, pitch, roll;
    float scale;  // > 0
};

struct Triangle {
    int16_t x0, y0, x1, y1, x2, y2;
    uint16_t color;
    bool wire;
};

enum Result : int {
    kBadMesh = -1,       // a header, size, face corner count or vertex index is wrong
    kBadTransform = -2,  // a non-finite or huge number, focal or scale <= 0
    kTooMany = -3,       // more than kMaxInstances or kMaxSceneFaces
    kNoMemory = -4,
};

// Writes up to `capacity` triangles, nearest last, and returns how many; the
// rest are counted in *dropped. A negative Result leaves `out` unspecified.
int project(const Camera &camera, const Instance *instances, int instance_count,
            Triangle *out, int capacity, int *dropped);

}  // namespace cyd::desktop::mesh
