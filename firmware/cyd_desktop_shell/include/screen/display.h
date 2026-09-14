#pragma once

// The ILI9341 panel on SPI2: bring-up, address window and raw transfers.
// Everything that reaches the LCD goes through transmit() or, for JPEG bands,
// the queued transfer in screen_jpeg.cpp; both update the SPI metrics below.

#include <cstddef>
#include <cstdint>

#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace cyd::desktop::screen {

constexpr int kWidth = 320;
constexpr int kHeight = 240;
// Every full-screen paint is composed in RAM one band of this many rows at a
// time, then sent as a single transfer, so the panel never shows a half-drawn
// frame.
constexpr int kTransitionTileRows = 16;
constexpr size_t kSolidBufferPixels = static_cast<size_t>(kWidth) * kTransitionTileRows;
constexpr int kDisplayDc = 2;
constexpr int kBacklight = 21;

extern spi_device_handle_t display;
// The band being composed (RGB565, byte-swapped for the panel).
extern uint16_t line_buffer[kSolidBufferPixels];
extern SemaphoreHandle_t display_mutex;

extern volatile uint32_t spi_transactions;
extern volatile uint64_t spi_bytes;
// Off by default: the CRC costs about a tenth of a native app's frame time.
// desktop_diagnostics {"fingerprint":1} turns it on until reboot.
extern volatile bool spi_fingerprint_enabled;
extern volatile uint32_t spi_fingerprint;

inline void delay_ms(uint32_t value) { vTaskDelay(pdMS_TO_TICKS(value)); }

inline uint16_t swap16(uint16_t value) { return static_cast<uint16_t>((value << 8) | (value >> 8)); }

bool transmit(const void *data, size_t length);
void command(uint8_t value);
void data(const uint8_t *values, size_t length);
void command_data(uint8_t value, const uint8_t *values, size_t length);
bool init_display();
void set_window(int x, int y, int w, int h);

}  // namespace cyd::desktop::screen
