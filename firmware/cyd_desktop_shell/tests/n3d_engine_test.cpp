// Host test for n3d_engine.cpp and the scene's polygon rasterizer, the pieces
// behind cyd.n3d_present().
#include "n3d_engine.h"
#include "scene_engine.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

using namespace cyd::desktop;

void put(std::vector<uint8_t> &out, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xff));
}

struct Face {
    std::vector<int> corners;
    uint16_t color;
    uint8_t flags = 0;
};

std::vector<uint8_t> model(const std::vector<std::vector<int>> &vertices, const std::vector<Face> &faces,
                           uint16_t model_flags = 0) {
    std::vector<uint8_t> out;
    put(out, static_cast<uint32_t>(vertices.size()), 2);
    put(out, static_cast<uint32_t>(faces.size()), 2);
    put(out, 100, 2);
    put(out, model_flags, 2);
    for (const auto &v : vertices)
        for (int c : v) put(out, static_cast<uint32_t>(c), 2);
    for (const Face &face : faces) {
        out.push_back(static_cast<uint8_t>(face.corners.size()));
        out.push_back(face.flags);
        put(out, face.color, 2);
        put(out, 0, 2);
        put(out, 0, 2);
        put(out, 0, 2);
        for (int k = 0; k < 4; ++k) put(out, k < static_cast<int>(face.corners.size()) ? face.corners[k] : 0, 2);
    }
    return out;
}

std::vector<uint8_t> camera(int ex, int ey, int ez, int ax, int ay, int az) {
    std::vector<uint8_t> out;
    for (int v : {ex, ey, ez, ax, ay, az}) put(out, static_cast<uint32_t>(v * 65536), 4);
    put(out, 0, 2);
    put(out, 0, 2);
    put(out, 0, 2);
    return out;
}

std::vector<uint8_t> item(int slot, int x, int y, int z, uint16_t flags = n3d::kEnable, uint16_t color = 0xffff,
                          uint8_t alpha = 255) {
    std::vector<uint8_t> out;
    put(out, static_cast<uint32_t>(slot), 2);
    put(out, flags, 2);
    put(out, 0, 2);
    put(out, 0, 2);
    put(out, 0, 2);
    put(out, 256, 2);  // scale 1.0
    put(out, static_cast<uint32_t>(x * 65536), 4);
    put(out, static_cast<uint32_t>(y * 65536), 4);
    put(out, static_cast<uint32_t>(z * 65536), 4);
    put(out, color, 2);
    out.push_back(alpha);
    out.push_back(0);
    assert(out.size() == n3d::kItemBytes);
    return out;
}

// A 40x40 square in the z=0 plane, both windings so one of them faces the camera.
const std::vector<std::vector<int>> kSquare = {{-20, -20, 0}, {20, -20, 0}, {20, 20, 0}, {-20, 20, 0}};

std::vector<uint16_t> screen() {
    std::vector<uint16_t> pixels(320 * 240, 0);
    for (int y = 0; y < 240; y += 16) {
        const native::TileSurface band{pixels.data() + y * 320, 320, 16, 320, y};
        scene::render(band);
    }
    return pixels;
}

uint16_t at(const std::vector<uint16_t> &pixels, int x, int y) { return native::byte_swap(pixels[y * 320 + x]); }

void flat(n3d::Light *light, n3d::Fog *fog) {
    *light = n3d::Light{0, 0, -16384, 255};  // full ambient: colours stay as given
    *fog = n3d::Fog{0, 0, 0};                // off
}

void geometry() {
    n3d::reset();
    assert(n3d::load_model(0, nullptr, 0) == n3d::kBadModel);
    assert(n3d::load_model(-1, nullptr, 0) == n3d::kBadSlot && n3d::load_model(n3d::kModelCapacity, nullptr, 0) == n3d::kBadSlot);
    auto bad = model(kSquare, {{{0, 1, 9}, 1}});
    assert(n3d::load_model(0, bad.data(), bad.size()) == n3d::kBadModel);
    bad = model(kSquare, {{{0, 1}, 1}});
    assert(n3d::load_model(0, bad.data(), bad.size()) == n3d::kBadModel);
    auto good = model(kSquare, {{{0, 1, 2, 3}, 0xf800}, {{3, 2, 1, 0}, 0x07e0}});
    assert(n3d::load_model(0, good.data(), good.size() - 1) == n3d::kBadModel);
    assert(n3d::load_model(0, good.data(), good.size()) == n3d::kOk && n3d::model_loaded(0));

    // The camera 240 in front of the origin, looking at it.
    int32_t matrix[12];
    const auto cam = camera(0, 0, -240, 0, 0, 0);
    n3d::set_view(cam.data(), matrix);
    assert(matrix[11] == 240 * 65536);  // the origin is 240 ahead

    n3d::Light light;
    n3d::Fog fog;
    flat(&light, &fog);
    n3d::set_light(light);
    n3d::set_fog(fog);
    n3d::set_projection(n3d::default_projection());

    scene::Poly polys[8];
    const auto square = item(0, 0, 0, 0);
    // Only the face wound towards the camera survives culling.
    assert(n3d::build_item(square.data(), false, polys, 8) == 1);
    const scene::Poly &poly = polys[0];
    assert(poly.count == 4 && poly.alpha == 255 && !(poly.flags & scene::kPolyWire));
    int min_x = 999, max_x = -999, min_y = 999, max_y = -999;
    for (int k = 0; k < 4; ++k) {
        min_x = std::min<int>(min_x, poly.x[k]);
        max_x = std::max<int>(max_x, poly.x[k]);
        min_y = std::min<int>(min_y, poly.y[k]);
        max_y = std::max<int>(max_y, poly.y[k]);
        assert(poly.z[k] == 65535 * 4 / 240);  // near/z depth key
    }
    // Centred on the default projection centre; 40 units at 240 with a 203 px focal ~ 34 px.
    assert(min_x + max_x == 2 * 160 && min_y + max_y == 2 * 127);
    assert(max_x - min_x >= 32 && max_x - min_x <= 36);
    // Wire frames keep both faces; a disabled item or an empty slot gives none.
    assert(n3d::build_item(square.data(), true, polys, 8) == 2 && (polys[0].flags & scene::kPolyWire));
    const auto off = item(0, 0, 0, 0, 0);
    assert(n3d::build_item(off.data(), false, polys, 8) == 0);
    const auto empty = item(5, 0, 0, 0);
    assert(n3d::build_item(empty.data(), false, polys, 8) == 0);
    // Translucent or additive items do not write depth.
    const auto ghost = item(0, 0, 0, 0, n3d::kEnable | n3d::kAdd, 0xffff, 128);
    assert(n3d::build_item(ghost.data(), false, polys, 8) == 1);
    assert((polys[0].flags & scene::kPolyAdd) && (polys[0].flags & scene::kPolyNoZWrite) && polys[0].alpha == 128);
    // A colour override replaces the face colour.
    const auto tinted = item(0, 0, 0, 0, n3d::kEnable, 0x001f);
    assert(n3d::build_item(tinted.data(), false, polys, 8) == 1 && polys[0].color == 0x001f);
    // Behind the camera: nothing.
    const auto behind = item(0, 0, 0, -400);
    assert(n3d::build_item(behind.data(), false, polys, 8) == 0);
    // Crossing the near plane: clipped, not dropped.
    const auto close = item(0, 0, 0, -238);
    std::vector<std::vector<int>> slab = {{-20, -20, -10}, {20, -20, -10}, {20, 20, 10}, {-20, 20, 10}};
    auto tilted = model(slab, {{{0, 1, 2, 3}, 0xf800}, {{3, 2, 1, 0}, 0x07e0}}, 1);
    assert(n3d::load_model(1, tilted.data(), tilted.size()) == n3d::kOk);
    auto near_item = item(1, 0, 0, -236);
    const int clipped = n3d::build_item(near_item.data(), false, polys, 8);
    assert(clipped >= 1);
    for (int k = 0; k < polys[0].count; ++k) assert(polys[0].z[k] >= 1);
    (void)close;

    // Points.
    std::vector<uint8_t> points;
    for (int v : {0, 0, 0, 0, 0, -400}) put(points, static_cast<uint32_t>(v * 65536), 4);
    uint8_t projected[12];
    assert(n3d::project_points(points.data(), 2, projected) == 1);
    assert((projected[0] | (projected[1] << 8)) == 160 && (projected[2] | (projected[3] << 8)) == 127);
    assert((projected[4] | (projected[5] << 8)) == 65535 * 4 / 240);
    assert(projected[10] == 0 && projected[11] == 0);  // behind: depth 0
}

void rasterizer() {
    assert(scene::begin() && scene::begin_3d() && scene::polys() != nullptr);
    scene::Poly *list = scene::polys();
    auto square = [](scene::Poly &p, int x0, int y0, int x1, int y1, uint16_t z, uint16_t color) {
        p = scene::Poly{};
        p.count = 4;
        p.x[0] = x0; p.y[0] = y0;
        p.x[1] = x1; p.y[1] = y0;
        p.x[2] = x1; p.y[2] = y1;
        p.x[3] = x0; p.y[3] = y1;
        for (int k = 0; k < 4; ++k) p.z[k] = z;
        p.color = color;
        p.alpha = 255;
    };
    // The far red square is listed after the near blue one; depth decides.
    square(list[0], 100, 10, 140, 40, 3000, 0x001f);
    square(list[1], 120, 20, 160, 50, 1000, 0xf800);
    assert(scene::prepare(nullptr, 0, 0x0000, 2, true) == 0);
    auto pixels = screen();
    assert(at(pixels, 110, 15) == 0x001f);   // blue only
    assert(at(pixels, 130, 30) == 0x001f);   // overlap: blue is nearer
    assert(at(pixels, 150, 45) == 0xf800);   // red only, across the band at y=32
    assert(at(pixels, 99, 15) == 0 && at(pixels, 170, 45) == 0);
    // Without the Z buffer the later polygon wins.
    assert(scene::prepare(nullptr, 0, 0x0000, 2, false) == 0);
    pixels = screen();
    assert(at(pixels, 130, 30) == 0xf800);
    // Additive, no depth write.
    list[1].flags = scene::kPolyAdd | scene::kPolyNoZWrite;
    assert(scene::prepare(nullptr, 0, 0x0000, 2, true) == 0);
    pixels = screen();
    assert(at(pixels, 130, 30) == 0x001f);  // behind blue: depth-tested away
    assert(at(pixels, 150, 45) == 0xf800);
    // A wire frame draws only edges.
    square(list[0], 10, 100, 50, 140, 3000, 0xffff);
    list[0].flags = scene::kPolyWire;
    assert(scene::prepare(nullptr, 0, 0x0000, 1, true) == 0);
    pixels = screen();
    assert(at(pixels, 10, 120) == 0xffff && at(pixels, 30, 100) == 0xffff && at(pixels, 30, 120) == 0);
    // Huge off-screen coordinates are clipped, not wrapped.
    square(list[0], -100000, -100000, 100000, 100000, 2000, 0x07e0);
    assert(scene::prepare(nullptr, 0, 0x0000, 1, true) == 0);
    pixels = screen();
    assert(at(pixels, 0, 0) == 0x07e0 && at(pixels, 319, 239) == 0x07e0);
    assert(scene::prepare(nullptr, 0, 0, scene::kPolyCapacity + 1, true) == scene::kNotReady);
    scene::end();
    assert(scene::polys() == nullptr);
}

}  // namespace

int main() {
    geometry();
    rasterizer();
    n3d::reset();
    std::puts("n3d_engine_test: all assertions passed");
    return 0;
}
