// Host test for scene_engine.cpp, the renderer behind cyd.sprite_present().
#include "scene_engine.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using namespace cyd::desktop;

constexpr uint16_t kBackground = 0x0010;
constexpr uint16_t kRed = 0xf800;
constexpr uint16_t kGreen = 0x07e0;
constexpr uint16_t kBlue = 0x001f;
constexpr uint16_t kWhite = 0xffff;

struct Record {
    uint16_t id = 0;
    int16_t x = 0, y = 0;
    uint16_t scale_x = 256, scale_y = 256;
    int16_t angle = 0;
    int16_t centre_x = 0, centre_y = 0;
    uint16_t depth = 0;
    uint8_t alpha = 255, blend = 0;
    uint16_t flags = 0;
};

void put16(std::vector<uint8_t> &out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xff));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

std::vector<uint8_t> pack(const std::vector<Record> &records) {
    std::vector<uint8_t> out;
    for (const Record &r : records) {
        put16(out, r.id);
        put16(out, static_cast<uint16_t>(r.x));
        put16(out, static_cast<uint16_t>(r.y));
        put16(out, r.scale_x);
        put16(out, r.scale_y);
        put16(out, static_cast<uint16_t>(r.angle));
        put16(out, static_cast<uint16_t>(r.centre_x));
        put16(out, static_cast<uint16_t>(r.centre_y));
        put16(out, r.depth);
        out.push_back(r.alpha);
        out.push_back(r.blend);
        put16(out, r.flags);
    }
    assert(out.size() == records.size() * scene::kRecordBytes);
    return out;
}

// The whole screen, rendered band by band like the device does.
std::vector<uint16_t> screen() {
    std::vector<uint16_t> pixels(native::kScreenWidth * native::kScreenHeight, 0);
    for (int y = 0; y < native::kScreenHeight; y += 16) {
        const native::TileSurface band{pixels.data() + static_cast<std::size_t>(y) * native::kScreenWidth,
                                       native::kScreenWidth, 16, native::kScreenWidth, y};
        scene::render(band);
    }
    return pixels;
}

uint16_t at(const std::vector<uint16_t> &pixels, int x, int y) {
    return native::byte_swap(pixels[static_cast<std::size_t>(y) * native::kScreenWidth + x]);
}

int present(const std::vector<Record> &records) {
    const auto bytes = pack(records);
    return scene::prepare(bytes.data(), bytes.size(), kBackground);
}

// Images, in native byte order like a little-endian RGB565 file on the ESP32.
const uint16_t kSquare[4] = {kRed, kGreen, kBlue, kWhite};     // 2x2
const uint16_t kBar[3] = {kRed, kGreen, kBlue};                // 3x1
const uint16_t kHole[4] = {kWhite, 0, kWhite, kWhite};         // 2x2, one transparent pixel

void direct_copy_and_transparency() {
    assert(present({{1, 10, 20}}) == 1);
    auto pixels = screen();
    assert(at(pixels, 10, 20) == kWhite && at(pixels, 11, 20) == 0x0010);  // 0 is transparent
    assert(at(pixels, 10, 21) == kWhite && at(pixels, 11, 21) == kWhite);
    assert(at(pixels, 9, 20) == kBackground && at(pixels, 12, 21) == kBackground);
    // The centre is the anchor: (x, y) is where the image's centre lands.
    assert(present({{0, 100, 100, 256, 256, 0, 1, 1}}) == 1);
    pixels = screen();
    assert(at(pixels, 99, 99) == kRed && at(pixels, 100, 100) == kWhite);
}

void scale_mirror_and_rotation() {
    assert(present({{0, 50, 50, 512, 512}}) == 1);  // 2x
    auto pixels = screen();
    assert(at(pixels, 50, 50) == kRed && at(pixels, 51, 51) == kRed);
    assert(at(pixels, 52, 50) == kGreen && at(pixels, 53, 51) == kGreen);
    assert(at(pixels, 50, 52) == kBlue && at(pixels, 53, 53) == kWhite);
    assert(at(pixels, 54, 50) == kBackground);

    assert(present({{2, 60, 60, 256, 256, 0, 0, 0, 0, 255, 0, 1}}) == 1);  // mirrored, anchored at the left
    pixels = screen();
    assert(at(pixels, 60, 60) == kRed && at(pixels, 59, 60) == kGreen && at(pixels, 58, 60) == kBlue);

    // A quarter turn clockwise of the 3x1 bar about its left pixel stands it up.
    assert(present({{2, 80, 80, 256, 256, 16384, 0, 0}}) == 1);
    pixels = screen();
    int column_pixels = 0;
    for (int y = 70; y < 90; ++y)
        for (int x = 70; x < 90; ++x)
            if (at(pixels, x, y) != kBackground) {
                ++column_pixels;
                assert(x == 79 || x == 80);
            }
    assert(column_pixels == 3);
}

void depth_alpha_and_blend() {
    // Depth 1 is in front of depth 5 whatever the record order.
    assert(present({{0, 30, 30, 256, 256, 0, 0, 0, 1}, {1, 30, 30, 256, 256, 0, 0, 0, 5}}) == 2);
    auto pixels = screen();
    assert(at(pixels, 30, 30) == kRed);
    // Half alpha over the blue background.
    assert(present({{0, 30, 30, 256, 256, 0, 0, 0, 0, 128}}) == 1);
    pixels = screen();
    const uint16_t mixed = at(pixels, 30, 30);
    assert(((mixed >> 11) & 0x1f) == 15 && (mixed & 0x1f) == 7);
    // Additive: red + the background's blue.
    assert(present({{0, 30, 30, 256, 256, 0, 0, 0, 0, 255, 1}}) == 1);
    pixels = screen();
    assert(at(pixels, 30, 30) == (kRed | kBackground));
    // Alpha 0 is skipped but still counted.
    assert(present({{0, 30, 30, 256, 256, 0, 0, 0, 0, 0}}) == 1);
    pixels = screen();
    assert(at(pixels, 30, 30) == kBackground);
}

void bad_records_keep_the_frame() {
    assert(present({{1, 5, 5}}) == 1);
    for (const Record &bad : {Record{9, 0, 0}, Record{31, 0, 0}, Record{0, 0, 0, 0},
                              Record{0, 0, 0, 256, 5000}, Record{0, 0, 0, 256, 256, 0, 3},
                              Record{0, 0, 0, 256, 256, 0, 0, 0, 0, 255, 4},
                              Record{0, 0, 0, 256, 256, 0, 0, 0, 0, 255, 0, 4}})
        assert(present({bad}) == scene::kBadRecords);
    const auto bytes = pack({{1, 5, 5}});
    assert(scene::prepare(bytes.data(), bytes.size() - 1, kBackground) == scene::kBadRecords);
    assert(at(screen(), 5, 5) == kWhite);
    std::vector<Record> too_many(scene::kItemCapacity + 1, Record{1, 0, 0});
    assert(present(too_many) == scene::kBadRecords);
    std::vector<Record> full(scene::kItemCapacity, Record{1, 0, 0});
    assert(present(full) == scene::kItemCapacity);
}

void overlay_shows_once() {
    assert(scene::overlay_add(1, 0, 230, 320, 10, kGreen, nullptr, 1, 0, 0));
    assert(scene::overlay_add(0, 160, 2, 0, 0, kWhite, "HI", 2, 0, 1));
    assert(present({}) == 0);
    auto pixels = screen();
    assert(at(pixels, 0, 230) == kGreen && at(pixels, 319, 239) == kGreen);
    int lit = 0;
    for (int y = 2; y < 16; ++y)
        for (int x = 148; x < 172; ++x)
            if (at(pixels, x, y) == kWhite) ++lit;
    assert(lit > 20);
    // The overlay belonged to that frame; the next one starts without it.
    assert(present({}) == 0);
    assert(at(screen(), 0, 230) == kBackground);
    for (int index = 0; index < scene::kOverlayCapacity; ++index)
        assert(scene::overlay_add(1, 0, 0, 1, 1, kRed, nullptr, 1, 0, 0));
    assert(!scene::overlay_add(1, 0, 0, 1, 1, kRed, nullptr, 1, 0, 0));
    scene::overlay_clear();
    assert(present({}) == 0 && at(screen(), 0, 0) == kBackground);
}

}  // namespace

int main() {
    assert(present({}) == scene::kNotReady);
    assert(scene::begin() && scene::ready());
    assert(!scene::set_image(-1, kSquare, 2, 2) && !scene::set_image(scene::kImageCapacity, kSquare, 2, 2));
    assert(!scene::set_image(0, kSquare, 0, 2) && !scene::set_image(0, kSquare, 321, 1));
    assert(scene::set_image(0, kSquare, 2, 2) && scene::set_image(1, kHole, 2, 2) && scene::set_image(2, kBar, 3, 1));
    direct_copy_and_transparency();
    scale_mirror_and_rotation();
    depth_alpha_and_blend();
    bad_records_keep_the_frame();
    overlay_shows_once();
    scene::end();
    assert(!scene::ready() && !scene::image_loaded(0));
    std::puts("scene_engine_test: all assertions passed");
    return 0;
}
