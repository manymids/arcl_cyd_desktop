// JPEG decoding straight into LCD bands: cyd.jpeg(), cyd.jpeg_file(), the
// SD streaming path and the video surface's cyd.video_present().

#include "screen/display.h"
#include "screen/jpeg.h"
#include "screen/theme.h"
#include "screen/views.h"
#include "native_app.h"
#include "shell_state.h"

extern "C" {
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_rom_crc.h"
#include "esp32/rom/tjpgd.h"
}

#include <algorithm>
#include <cstdio>
#include <cstring>

extern "C" void cyd_desktop_ui_refresh(void);

namespace cyd::desktop::screen {

namespace {

constexpr int kVideoWidth = 240;
constexpr int kVideoX = (kWidth - kVideoWidth) / 2;

constexpr size_t kJpegWorkBytes = 4096;
// Allocated on first use; see screen/jpeg.h. The band is sent by DMA.
uint16_t *jpeg_second_band = nullptr;
uint8_t *jpeg_work = nullptr;

bool jpeg_acquire_buffers() {
    if (jpeg_second_band == nullptr) {
        jpeg_second_band = static_cast<uint16_t *>(heap_caps_malloc(
            kSolidBufferPixels * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    }
    if (jpeg_work == nullptr) {
        jpeg_work = static_cast<uint8_t *>(heap_caps_malloc(kJpegWorkBytes, MALLOC_CAP_8BIT));
    }
    return jpeg_second_band != nullptr && jpeg_work != nullptr;
}
bool video_surface_prepared = false;

struct JpegContext {
    const uint8_t *data;
    size_t size;
    size_t offset;
    FILE *file;
    void *user_data;
    int (*stream_read)(void *user_data, uint8_t *dest, size_t requested);
    int (*stream_seek)(void *user_data, size_t offset);
    int draw_width;
    int draw_height;
    int offset_x;
    int offset_y;
    bool output_failed;
    uint8_t queued;
    spi_transaction_t transactions[2];
};

uint16_t *jpeg_band_buffer(int slot) {
    return slot == 0 ? line_buffer : jpeg_second_band;
}

bool jpeg_wait_one(JpegContext *input) {
    if (input->queued == 0) return true;
    spi_transaction_t *completed = nullptr;
    if (spi_device_get_trans_result(display, &completed, portMAX_DELAY) != ESP_OK) return false;
    --input->queued;
    return completed != nullptr;
}

void prepare_video_surface() {
    std::fill(line_buffer, line_buffer + kSolidBufferPixels, swap16(kBlack));
    set_window(0, 0, kWidth, kHeight);
    gpio_set_level(static_cast<gpio_num_t>(kDisplayDc), 1);
    for (int y = 0; y < kHeight; y += kTransitionTileRows)
        transmit(line_buffer, static_cast<size_t>(kWidth) * kTransitionTileRows * sizeof(uint16_t));
    video_surface_prepared = true;
}

UINT jpeg_mem_input(JDEC *decoder, BYTE *destination, UINT requested) {
    auto *ctx = static_cast<JpegContext *>(decoder->device);
    const size_t available = ctx->offset < ctx->size ? ctx->size - ctx->offset : 0;
    const size_t count = std::min<size_t>(requested, available);
    if (destination != nullptr && count > 0)
        std::memcpy(destination, ctx->data + ctx->offset, count);
    ctx->offset += count;
    return static_cast<UINT>(count);
}

UINT jpeg_file_input(JDEC *decoder, BYTE *destination, UINT requested) {
    auto *ctx = static_cast<JpegContext *>(decoder->device);
    if (ctx->file == nullptr) return 0;
    if (destination != nullptr) {
        return static_cast<UINT>(std::fread(destination, 1, requested, ctx->file));
    } else {
        std::fseek(ctx->file, requested, SEEK_CUR);
        return requested;
    }
}

UINT jpeg_stream_input(JDEC *decoder, BYTE *destination, UINT requested) {
    auto *ctx = static_cast<JpegContext *>(decoder->device);
    if (ctx == nullptr) return 0;
    if (destination != nullptr) {
        if (ctx->stream_read == nullptr) return 0;
        const int count = ctx->stream_read(ctx->user_data, destination, requested);
        return count > 0 ? static_cast<UINT>(count) : 0;
    } else {
        if (ctx->stream_seek == nullptr) return 0;
        const int skipped = ctx->stream_seek(ctx->user_data, requested);
        return skipped > 0 ? static_cast<UINT>(skipped) : 0;
    }
}

UINT jpeg_output(JDEC *decoder, void *bitmap, JRECT *rect) {
    auto *ctx = static_cast<JpegContext *>(decoder->device);
    const int block_width = rect->right - rect->left + 1;
    const int block_height = rect->bottom - rect->top + 1;
    if (block_height > kTransitionTileRows) {
        ctx->output_failed = true;
        return 0;
    }
    const int slot = (rect->top / kTransitionTileRows) & 1;
    if (rect->left == 0 && ctx->queued == 2 && !jpeg_wait_one(ctx)) {
        ctx->output_failed = true;
        return 0;
    }
    uint16_t *band = jpeg_band_buffer(slot);
    const auto *source = static_cast<const uint8_t *>(bitmap);
    for (int row = 0; row < block_height; ++row) {
        uint16_t *destination = band + static_cast<size_t>(row) * ctx->draw_width + rect->left;
        for (int column = 0; column < block_width; ++column) {
            const size_t source_index = (static_cast<size_t>(row) * block_width + column) * 3;
            const uint16_t color = static_cast<uint16_t>(((source[source_index] & 0xf8) << 8) |
                ((source[source_index + 1] & 0xfc) << 3) | (source[source_index + 2] >> 3));
            destination[column] = swap16(color);
        }
    }
    if (rect->right == ctx->draw_width - 1) {
        spi_transaction_t &transaction = ctx->transactions[slot];
        std::memset(&transaction, 0, sizeof(transaction));
        transaction.length = static_cast<size_t>(ctx->draw_width) * block_height * sizeof(uint16_t) * 8;
        transaction.tx_buffer = band;
        if (spi_device_queue_trans(display, &transaction, portMAX_DELAY) != ESP_OK) {
            ctx->output_failed = true;
            return 0;
        }
        ++ctx->queued;
        ++spi_transactions;
        spi_bytes += transaction.length / 8;
        if (spi_fingerprint_enabled)
            spi_fingerprint += esp_rom_crc32_le(0, static_cast<const uint8_t *>(transaction.tx_buffer),
                                                transaction.length / 8);
    }
    return 1;
}

int render_jpeg_context(JpegContext &ctx, UINT (*in_func)(JDEC*, BYTE*, UINT), int target_x, int target_y, bool clear_background) {
    if (display_mutex == nullptr) return -1;
    if (xSemaphoreTake(display_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) return -2;
    if (!jpeg_acquire_buffers()) {
        jpeg_release_buffers();
        xSemaphoreGive(display_mutex);
        return -7;
    }

    JDEC decoder{};
    JRESULT result = jd_prepare(&decoder, in_func, jpeg_work, kJpegWorkBytes, &ctx);
    if (result != JDR_OK) {
        xSemaphoreGive(display_mutex);
        return -static_cast<int>(result) - 10;
    }

    uint8_t scale = 0;
    while (scale < 3 && ((decoder.width >> scale) > kWidth || (decoder.height >> scale) > kHeight)) {
        ++scale;
    }

    ctx.draw_width = decoder.width >> scale;
    ctx.draw_height = decoder.height >> scale;

    // TJpgDec scales by at most 1/8, so an image wider than 8 * kWidth still
    // does not fit. draw_width is the row stride into a kSolidBufferPixels
    // band and the width handed to set_window, so refuse rather than stride
    // past the buffer and open a window wider than the panel.
    // JPEG result codes, as cyd.jpeg() / cyd.video_present() see them:
    //   -1 bad argument   -2 display busy   -3 LCD output failed
    //   -4 not in the video view (video_present only; reads as "stopped")
    //   -5 file could not be opened (jpeg from a path)
    //   -6 image larger than the panel even at 1/8 scale
    //   -7 not enough memory for the decoder's buffers
    //   -11 and below: TJpgDec error, -(JRESULT) - 10
    // Check this list before adding a code: -4 and then -5 were both reused once.
    if (ctx.draw_width > kWidth || ctx.draw_height > kHeight) {
        xSemaphoreGive(display_mutex);
        return -6;
    }

    if (target_x < 0) ctx.offset_x = (kWidth - ctx.draw_width) / 2;
    else ctx.offset_x = std::max(0, std::min(kWidth - ctx.draw_width, target_x));

    if (target_y < 0) ctx.offset_y = (kHeight - ctx.draw_height) / 2;
    else ctx.offset_y = std::max(0, std::min(kHeight - ctx.draw_height, target_y));

    if (clear_background && (ctx.draw_width < kWidth || ctx.draw_height < kHeight)) {
        prepare_video_surface();
    }

    set_window(ctx.offset_x, ctx.offset_y, ctx.draw_width, ctx.draw_height);
    gpio_set_level(static_cast<gpio_num_t>(kDisplayDc), 1);

    result = jd_decomp(&decoder, jpeg_output, scale);

    while (ctx.queued > 0) {
        if (!jpeg_wait_one(&ctx)) ctx.output_failed = true;
    }
    xSemaphoreGive(display_mutex);

    if (result != JDR_OK || ctx.output_failed) return result == JDR_OK ? -3 : -static_cast<int>(result) - 10;
    ++render_count;
    return 0;
}

int present_jpeg(const uint8_t *jpeg, size_t size) {
    if (jpeg == nullptr || size < 4) return -1;
    if (!video_surface_prepared) prepare_video_surface();
    const TickType_t started = xTaskGetTickCount();
    JpegContext ctx{};
    ctx.data = jpeg;
    ctx.size = size;
    ctx.offset = 0;
    ctx.file = nullptr;
    const int res = render_jpeg_context(ctx, jpeg_mem_input, kVideoX, 0, false);
    if (res == 0) {
        const uint32_t elapsed = static_cast<uint32_t>(xTaskGetTickCount() - started) * portTICK_PERIOD_MS;
        cyd::desktop::native::note_frame(elapsed);
    }
    return res;
}

}  // namespace

}  // namespace cyd::desktop::screen

namespace cyd::desktop::screen {

void jpeg_release_buffers() {
    heap_caps_free(jpeg_second_band);
    heap_caps_free(jpeg_work);
    jpeg_second_band = nullptr;
    jpeg_work = nullptr;
}

}  // namespace cyd::desktop::screen

using namespace cyd::desktop::screen;
namespace {

// A single image does not keep the decoder's buffers.
int finish_single_image(int result) {
    if (display_mutex != nullptr && xSemaphoreTake(display_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        jpeg_release_buffers();
        xSemaphoreGive(display_mutex);
    }
    return result;
}

}  // namespace


extern "C" bool cyd_desktop_video_begin(void) {
    const auto *video = cyd::desktop::native::find_with_flag(cyd::desktop::native::kAppVideoSurface);
    if (video == nullptr || !cyd::desktop::shell_state().launch_native(video->id)) return false;
    video_surface_prepared = false;
    cyd_desktop_ui_refresh();
    return cyd::desktop::native::active() && cyd::desktop::native::current() == video;
}

extern "C" int cyd_desktop_video_present(const uint8_t *jpeg, size_t size) {
    const auto snapshot = cyd::desktop::shell_state().snapshot();
    if (snapshot.view != cyd::desktop::View::Native ||
        !cyd::desktop::native::has_flag(snapshot.foreground_app, cyd::desktop::native::kAppVideoSurface)) return -4;
    return present_jpeg(jpeg, size);
}

extern "C" int cyd_desktop_jpeg_present(const uint8_t *jpeg, size_t size, int x, int y) {
    if (jpeg == nullptr || size < 4) return -1;
    JpegContext ctx{};
    ctx.data = jpeg;
    ctx.size = size;
    ctx.offset = 0;
    ctx.file = nullptr;
    const bool clear_bg = (x < 0 && y < 0);
    return finish_single_image(render_jpeg_context(ctx, jpeg_mem_input, x, y, clear_bg));
}

extern "C" int cyd_desktop_jpeg_file(const char *path, int x, int y) {
    if (path == nullptr || path[0] == '\0') return -1;
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) return -5;
    JpegContext ctx{};
    ctx.data = nullptr;
    ctx.size = 0;
    ctx.offset = 0;
    ctx.file = file;
    const bool clear_bg = (x < 0 && y < 0);
    const int result = render_jpeg_context(ctx, jpeg_file_input, x, y, clear_bg);
    std::fclose(file);
    return finish_single_image(result);
}

extern "C" int cyd_desktop_jpeg_stream(void *user_data,
                                       int (*read_fn)(void *user_data, uint8_t *dest, size_t requested),
                                       int (*seek_fn)(void *user_data, size_t offset),
                                       int x, int y) {
    if (read_fn == nullptr || seek_fn == nullptr) return -1;
    JpegContext ctx{};
    ctx.user_data = user_data;
    ctx.stream_read = read_fn;
    ctx.stream_seek = seek_fn;
    const bool clear_bg = (x < 0 && y < 0);
    return finish_single_image(render_jpeg_context(ctx, jpeg_stream_input, x, y, clear_bg));
}

