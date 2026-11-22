// Host test for mesh_engine.cpp, the projection behind cyd.game_meshes().
#include "mesh_engine.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using namespace cyd::desktop::mesh;

void put16(std::vector<uint8_t> &out, int value) {
    out.push_back(static_cast<uint8_t>(value & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

struct Face {
    uint16_t color;
    std::vector<int> corners;
};

std::vector<uint8_t> mesh(const std::vector<std::vector<int>> &vertices, const std::vector<Face> &faces) {
    std::vector<uint8_t> out;
    put16(out, static_cast<int>(vertices.size()));
    put16(out, static_cast<int>(faces.size()));
    for (const auto &v : vertices) {
        put16(out, v[0]);
        put16(out, v[1]);
        put16(out, v[2]);
    }
    for (const Face &face : faces) {
        put16(out, face.color);
        out.push_back(static_cast<uint8_t>(face.corners.size()));
        for (int k = 0; k < 4; ++k) out.push_back(static_cast<uint8_t>(k < static_cast<int>(face.corners.size()) ? face.corners[k] : 0));
    }
    return out;
}

Instance at(const std::vector<uint8_t> &data, float z, float scale = 1.0f) {
    return Instance{data.data(), data.size(), 0, 0, z, 0, 0, 0, scale};
}

const Camera kCamera{0, 0, 0, 0, 0, 0, 100, false};

void projection_matches_the_pinhole_model() {
    // One triangle, wound so that it faces the camera.
    const auto data = mesh({{0, 0, 0}, {10, 0, 0}, {0, 10, 0}}, {{0xf800, {0, 1, 2}}});
    Triangle out[4];
    int dropped = -1;
    const Instance instance = at(data, 100);
    assert(project(kCamera, &instance, 1, out, 4, &dropped) == 1 && dropped == 0);
    // (0,0,100) lands on the centre; +y is up the screen; focal 100 at z 100 is 1:1.
    assert(out[0].x0 == kCentreX && out[0].y0 == kCentreY);
    assert(out[0].x1 == kCentreX + 10 && out[0].y1 == kCentreY);
    assert(out[0].x2 == kCentreX && out[0].y2 == kCentreY - 10);
    assert(out[0].color == 0xf800 && !out[0].wire);
    // Twice as far is half the size; scale 2 restores it.
    const Instance far = at(data, 200);
    assert(project(kCamera, &far, 1, out, 4, nullptr) == 1 && out[0].x1 == kCentreX + 5);
    const Instance scaled = at(data, 200, 2.0f);
    assert(project(kCamera, &scaled, 1, out, 4, nullptr) == 1 && out[0].x1 == kCentreX + 10);
}

void culling_clipping_and_wire() {
    const auto back = mesh({{0, 0, 0}, {0, 10, 0}, {10, 0, 0}}, {{1, {0, 1, 2}}});
    Triangle out[4];
    Instance instance = at(back, 100);
    assert(project(kCamera, &instance, 1, out, 4, nullptr) == 0);  // facing away
    Camera wire = kCamera;
    wire.wire = true;
    assert(project(wire, &instance, 1, out, 4, nullptr) == 1 && out[0].wire);
    // A face with a corner at or behind the near plane is dropped whole.
    instance = at(back, kNearZ - 1);
    assert(project(wire, &instance, 1, out, 4, nullptr) == 0);
}

void quads_sorting_and_capacity() {
    // A near quad (red) and a far quad (blue) in one instance: two triangles each,
    // furthest first.
    const auto data = mesh({{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0},
                            {0, 0, 300}, {10, 0, 300}, {10, 10, 300}, {0, 10, 300}},
                           {{0xf800, {0, 1, 2, 3}}, {0x001f, {4, 5, 6, 7}}});
    Triangle out[8];
    int dropped = 0;
    const Instance instance = at(data, 100);
    assert(project(kCamera, &instance, 1, out, 8, &dropped) == 4 && dropped == 0);
    assert(out[0].color == 0x001f && out[1].color == 0x001f);
    assert(out[2].color == 0xf800 && out[3].color == 0xf800);
    assert(project(kCamera, &instance, 1, out, 3, &dropped) == 3 && dropped == 1);
}

void bad_input_is_refused() {
    Triangle out[4];
    const auto good = mesh({{0, 0, 0}, {0, 10, 0}, {10, 0, 0}}, {{1, {0, 1, 2}}});
    Instance instance = at(good, 100);
    Camera camera = kCamera;
    camera.focal = 0;
    assert(project(camera, &instance, 1, out, 4, nullptr) == kBadTransform);
    camera = kCamera;
    camera.yaw = 1.0f / 0.0f;
    assert(project(camera, &instance, 1, out, 4, nullptr) == kBadTransform);
    instance.scale = 0;
    assert(project(kCamera, &instance, 1, out, 4, nullptr) == kBadTransform);
    auto short_mesh = good;
    short_mesh.pop_back();
    instance = at(short_mesh, 100);
    assert(project(kCamera, &instance, 1, out, 4, nullptr) == kBadMesh);
    const auto bad_index = mesh({{0, 0, 0}, {0, 10, 0}, {10, 0, 0}}, {{1, {0, 1, 7}}});
    instance = at(bad_index, 100);
    assert(project(kCamera, &instance, 1, out, 4, nullptr) == kBadMesh);
    const auto bad_corners = mesh({{0, 0, 0}, {0, 10, 0}, {10, 0, 0}}, {{1, {0, 1}}});
    instance = at(bad_corners, 100);
    assert(project(kCamera, &instance, 1, out, 4, nullptr) == kBadMesh);
    std::vector<Instance> many(kMaxInstances + 1, at(good, 100));
    assert(project(kCamera, many.data(), static_cast<int>(many.size()), out, 4, nullptr) == kTooMany);
    assert(project(kCamera, nullptr, 0, out, 4, nullptr) == 0);
}

}  // namespace

int main() {
    projection_matches_the_pinhole_model();
    culling_clipping_and_wire();
    quads_sorting_and_capacity();
    bad_input_is_refused();
    std::puts("mesh_engine_test: all assertions passed");
    return 0;
}
