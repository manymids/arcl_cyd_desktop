// The tile map scene behind cyd.tile_*() and the script-hosted native view that
// shows it. See include/tile_engine.h. Plain C++ so the host tests can render
// it; screen_tiles.cpp puts it on the panel.

#include "tile_engine.h"
#include "ui_font.h"

#include <algorithm>
#include <cstring>

namespace cyd::desktop::tiles {

namespace {

struct Sprite {
    int16_t x;
    int16_t y;
    uint16_t cell;
    uint8_t flags;
};

struct Text {
    int16_t x;
    int16_t y;
    uint8_t color;
    uint8_t scale;
    char value[kTextLength + 1];
};

struct FrameState {
    int scroll_x;
    uint8_t sprite_count;
    Sprite sprites[kSpriteCapacity];
    Text texts[kTextCapacity];
};

struct Scene {
    const uint8_t *tiles;
    std::size_t tile_count;
    const uint8_t *sprites;
    std::size_t sprite_cells;
    const uint8_t *map;
    int map_width;
    int map_height;
    int origin_y;
    int palette_count;
    // Byte-swapped for the panel, like TileSurface pixels.
    uint16_t palette[kMaxPalettes * kPaletteColors];
};

Scene scene{};
FrameState pending{};
FrameState shown{};

int adopt_palette(const uint8_t *palette, std::size_t size, int expected_count) {
    if (palette == nullptr || size == 0 || size % (kPaletteColors * 2) != 0) return kBadPalette;
    const int count = static_cast<int>(size / (kPaletteColors * 2));
    if (count > kMaxPalettes || (expected_count != 0 && count != expected_count)) return kBadPalette;
    for (int index = 0; index < count * kPaletteColors; ++index) {
        // Big-endian RGB565 in the buffer; the panel takes the same byte order.
        const uint16_t value = static_cast<uint16_t>((palette[index * 2] << 8) | palette[index * 2 + 1]);
        scene.palette[index] = native::byte_swap(value);
    }
    scene.palette_count = count;
    return kOk;
}

inline uint8_t nibble(const uint8_t *row, int column) {
    const uint8_t byte = row[column >> 1];
    return (column & 1) ? (byte & 0x0f) : (byte >> 4);
}

// The palette-0 index the map shows at a world pixel; 0 outside the map.
uint8_t map_pixel(int world_x, int screen_y) {
    const int map_y = screen_y - scene.origin_y;
    if (world_x < 0 || map_y < 0) return 0;
    const int column = world_x >> 4;
    const int row = map_y >> 4;
    if (column >= scene.map_width || row >= scene.map_height) return 0;
    const uint8_t tile = scene.map[row * scene.map_width + column];
    if (tile == 0 || tile >= scene.tile_count) return 0;
    return nibble(scene.tiles + tile * kCellBytes + (map_y & 15) * 8, world_x & 15);
}

void render_map_row(uint16_t *destination, int width, int screen_y) {
    const uint16_t background = scene.palette[0];
    const int map_y = screen_y - scene.origin_y;
    if (map_y < 0 || map_y >= scene.map_height * kCell) {
        std::fill(destination, destination + width, background);
        return;
    }
    const uint8_t *map_row = scene.map + (map_y >> 4) * scene.map_width;
    const int tile_y = map_y & 15;
    int world_x = shown.scroll_x;
    for (int x = 0; x < width;) {
        const int pixel = world_x & 15;
        const int run = std::min(kCell - pixel, width - x);
        const int column = world_x >> 4;  // arithmetic shift: negative stays outside
        const uint8_t tile = column >= 0 && column < scene.map_width ? map_row[column] : 0;
        if (tile == 0 || tile >= scene.tile_count) {
            std::fill(destination + x, destination + x + run, background);
        } else {
            const uint8_t *source = scene.tiles + tile * kCellBytes + tile_y * 8;
            for (int step = 0; step < run; ++step)
                destination[x + step] = scene.palette[nibble(source, pixel + step)];
        }
        x += run;
        world_x += run;
    }
}

void render_sprite(const native::TileSurface &surface, const Sprite &sprite) {
    const int top = std::max(static_cast<int>(sprite.y), surface.origin_y);
    const int bottom = std::min(sprite.y + kCell, surface.origin_y + surface.height);
    if (top >= bottom || sprite.x <= -kCell || sprite.x >= surface.width) return;
    const uint16_t *colors = scene.palette + ((sprite.flags >> 4) % scene.palette_count) * kPaletteColors;
    const uint8_t *cell = scene.sprites + sprite.cell * kCellBytes;
    const int left = std::max(0, -static_cast<int>(sprite.x));
    const int right = std::min(kCell, surface.width - sprite.x);
    for (int y = top; y < bottom; ++y) {
        const int cell_y = (sprite.flags & kFlipY) ? kCell - 1 - (y - sprite.y) : y - sprite.y;
        const uint8_t *source = cell + cell_y * 8;
        uint16_t *row = surface.pixels + static_cast<std::size_t>(y - surface.origin_y) * surface.stride;
        for (int cell_x = left; cell_x < right; ++cell_x) {
            const uint8_t color = nibble(source, (sprite.flags & kFlipX) ? kCell - 1 - cell_x : cell_x);
            if (color == 0) continue;
            const int x = sprite.x + cell_x;
            if ((sprite.flags & kBehind) && map_pixel(x + shown.scroll_x, y) != 0) continue;
            row[x] = colors[color];
        }
    }
}

void render_text(const native::TileSurface &surface, const Text &text) {
    const int scale = text.scale;
    if (text.y + 7 * scale <= surface.origin_y || text.y >= surface.origin_y + surface.height) return;
    const uint16_t color = scene.palette[text.color % kPaletteColors];
    int x = text.x;
    for (const char *character = text.value; *character != '\0'; ++character, x += 6 * scale) {
        const unsigned char code = static_cast<unsigned char>(*character);
        const uint8_t *columns = ui::kFont5x7[(code >= 32 && code <= 126 ? code : '?') - 32];
        for (int column = 0; column < 5; ++column) {
            for (int row = 0; row < 7; ++row) {
                if (!(columns[column] & (1 << row))) continue;
                for (int dy = 0; dy < scale; ++dy)
                    surface.hline(x + column * scale, text.y + row * scale + dy, scale, native::byte_swap(color));
            }
        }
    }
}

}  // namespace

int begin(const uint8_t *palette, std::size_t palette_size,
          const uint8_t *tile_data, std::size_t tiles_size,
          const uint8_t *sprite_data, std::size_t sprites_size,
          const uint8_t *map, std::size_t map_size, int map_width, int map_height,
          int origin_y) {
    end();
    if (tile_data == nullptr || tiles_size < kCellBytes || tiles_size % kCellBytes != 0 ||
        tiles_size / kCellBytes > 256) return kBadTiles;
    if (sprite_data == nullptr || sprites_size < kCellBytes || sprites_size % kCellBytes != 0 ||
        sprites_size / kCellBytes > 65535) return kBadSprites;
    if (map == nullptr || map_width <= 0 || map_height <= 0 || map_height > kMaxMapHeight ||
        map_width > 4096 || map_size < static_cast<std::size_t>(map_width) * map_height ||
        origin_y < -kMaxMapHeight * kCell || origin_y > 240) return kBadMap;
    const int palette_result = adopt_palette(palette, palette_size, 0);
    if (palette_result != kOk) return palette_result;
    scene.tiles = tile_data;
    scene.tile_count = tiles_size / kCellBytes;
    scene.sprites = sprite_data;
    scene.sprite_cells = sprites_size / kCellBytes;
    scene.map = map;
    scene.map_width = map_width;
    scene.map_height = map_height;
    scene.origin_y = origin_y;
    pending = {};
    shown = {};
    return kOk;
}

void end() {
    scene = {};
    pending = {};
    shown = {};
}

bool ready() { return scene.tiles != nullptr; }

int set_palette(const uint8_t *palette, std::size_t palette_size) {
    if (!ready()) return kBadPalette;
    return adopt_palette(palette, palette_size, scene.palette_count);
}

void frame(int scroll_x) {
    pending.scroll_x = scroll_x;
    pending.sprite_count = 0;
}

bool sprite(int cell, int x, int y, uint8_t flags) {
    if (!ready() || cell < 0 || static_cast<std::size_t>(cell) >= scene.sprite_cells ||
        pending.sprite_count >= kSpriteCapacity) return false;
    if (x <= -kCell || x >= 320 || y <= -kCell || y >= 240) return true;  // off screen: nothing to draw
    pending.sprites[pending.sprite_count++] = {
        static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<uint16_t>(cell), flags,
    };
    return true;
}

bool text(int slot, int x, int y, const char *value, uint8_t color, int scale) {
    if (slot < 0 || slot >= kTextCapacity || value == nullptr) return false;
    Text &entry = pending.texts[slot];
    entry.x = static_cast<int16_t>(std::max(-320, std::min(640, x)));
    entry.y = static_cast<int16_t>(std::max(-240, std::min(480, y)));
    entry.color = static_cast<uint8_t>(color % kPaletteColors);
    entry.scale = static_cast<uint8_t>(std::max(1, std::min(4, scale)));
    std::strncpy(entry.value, value, kTextLength);
    entry.value[kTextLength] = '\0';
    return true;
}

void commit() { shown = pending; }

void render(const native::TileSurface &surface) {
    if (!ready()) {
        surface.fill(0);
        return;
    }
    for (int row = 0; row < surface.height; ++row) {
        render_map_row(surface.pixels + static_cast<std::size_t>(row) * surface.stride,
                       surface.width, surface.origin_y + row);
    }
    for (uint8_t index = 0; index < shown.sprite_count; ++index) render_sprite(surface, shown.sprites[index]);
    for (const Text &entry : shown.texts) {
        if (entry.value[0] != '\0') render_text(surface, entry);
    }
}

}  // namespace cyd::desktop::tiles

namespace cyd::desktop::native {

namespace {

void enter() {}
void leave() { tiles::end(); }
// The script presents frames itself; the shell only repaints on request.
bool update(uint32_t) { return false; }
void input(const InputEvent &) {}

// Not launchable by id: cyd.tile_begin() enters it with the app's scene.
const NativeApp app{
    "tilemap", "TILE MAP", 1000,
    enter, leave, update, input, tiles::render,
    kAppScriptHosted | kAppTileSurface | kAppFullScreen, nullptr, 0, LauncherIcon::None, nullptr,
};

}  // namespace

const NativeApp &tile_map_app() { return app; }

}  // namespace cyd::desktop::native
