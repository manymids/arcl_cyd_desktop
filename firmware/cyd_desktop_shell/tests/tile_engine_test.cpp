#include "tile_engine.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

using namespace cyd::desktop;

constexpr uint16_t kSky = 0x1234;
constexpr uint16_t kStone = 0xabcd;
constexpr uint16_t kRed = 0xf800;

std::vector<uint8_t> palette() {
    std::vector<uint8_t> bytes(32, 0);
    bytes[0] = kSky >> 8; bytes[1] = kSky & 0xff;      // colour 0
    bytes[2] = kStone >> 8; bytes[3] = kStone & 0xff;  // colour 1
    bytes[4] = kRed >> 8; bytes[5] = kRed & 0xff;      // colour 2
    return bytes;
}

// Reads a band pixel back as plain RGB565.
uint16_t at(const std::vector<uint16_t> &pixels, int x, int y) {
    return native::byte_swap(pixels[static_cast<std::size_t>(y) * native::kScreenWidth + x]);
}

std::vector<uint16_t> render_band(int origin_y) {
    std::vector<uint16_t> pixels(native::kScreenWidth * 16, 0);
    tiles::render({pixels.data(), native::kScreenWidth, 16, native::kScreenWidth, origin_y});
    return pixels;
}

void rejects_bad_buffers() {
    const auto colors = palette();
    std::vector<uint8_t> cells(256, 0x11), sprite(128, 0), map(4, 0);
    assert(tiles::begin(colors.data(), 31, cells.data(), 256, sprite.data(), 128, map.data(), 4, 2, 2, 0) == tiles::kBadPalette);
    assert(tiles::begin(colors.data(), 32, cells.data(), 100, sprite.data(), 128, map.data(), 4, 2, 2, 0) == tiles::kBadTiles);
    assert(tiles::begin(colors.data(), 32, cells.data(), 256, sprite.data(), 0, map.data(), 4, 2, 2, 0) == tiles::kBadSprites);
    assert(tiles::begin(colors.data(), 32, cells.data(), 256, sprite.data(), 128, map.data(), 3, 2, 2, 0) == tiles::kBadMap);
    assert(!tiles::ready());
}

void scrolls_the_map_and_draws_sprites() {
    const auto colors = palette();
    // Tile 1 is solid colour 1.
    std::vector<uint8_t> cells(256, 0x00);
    std::memset(cells.data() + 128, 0x11, 128);
    // Sprite 0: left half colour 2, right half transparent.
    std::vector<uint8_t> sprite(128, 0x00);
    for (int row = 0; row < 16; ++row) std::memset(sprite.data() + row * 8, 0x22, 4);
    // 30 x 2 map, row 1 all stone, row 0 stone only at column 2.
    std::vector<uint8_t> map(60, 0);
    map[2] = 1;
    for (int column = 0; column < 30; ++column) map[30 + column] = 1;
    assert(tiles::begin(colors.data(), colors.size(), cells.data(), cells.size(), sprite.data(), sprite.size(),
                        map.data(), map.size(), 30, 2, 16) == tiles::kOk);
    assert(tiles::ready());

    tiles::frame(8);
    assert(tiles::sprite(0, 100, 20, 0));
    assert(!tiles::sprite(1, 0, 0, 0));  // no such cell
    assert(tiles::text(0, 0, 0, "HI", 2, 1));
    // Nothing is shown before commit.
    auto top = render_band(0);
    assert(at(top, 0, 0) == kSky);
    tiles::commit();

    top = render_band(0);
    assert(at(top, 0, 0) == kRed);          // the left stroke of "H"
    assert(at(top, 1, 0) == kSky);
    assert(at(top, 200, 15) == kSky);       // above the map

    auto row0 = render_band(16);
    assert(at(row0, 23, 0) == kSky);        // column 2 spans world 32-47, screen 24-39 at scroll 8
    assert(at(row0, 24, 0) == kStone);
    assert(at(row0, 40, 0) == kSky);
    assert(at(row0, 100, 4) == kRed);       // sprite, left half
    assert(at(row0, 110, 4) == kSky);       // sprite, transparent half

    auto row1 = render_band(32);
    assert(at(row1, 0, 0) == kStone && at(row1, 319, 15) == kStone);

    // Behind: hidden where the map is not colour 0.
    tiles::frame(8);
    assert(tiles::sprite(0, 22, 16, tiles::kBehind | tiles::kFlipX));
    assert(tiles::sprite(0, 36, 16, tiles::kBehind | tiles::kFlipX));
    tiles::commit();
    row0 = render_band(16);
    assert(at(row0, 24, 0) == kStone);      // flipped: colour 2 on the right, behind stone
    assert(at(row0, 34, 0) == kStone);
    assert(at(row0, 22, 0) == kSky);        // flipped: the transparent half
    assert(at(row0, 44, 0) == kRed);        // behind, but over open sky

    tiles::end();
    assert(!tiles::ready());
}

}  // namespace

int main() {
    rejects_bad_buffers();
    scrolls_the_map_and_draws_sprites();
    return 0;
}
