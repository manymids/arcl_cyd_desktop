#include "screen/display.h"

extern "C" {
#include "driver/gpio.h"
#include "esp_rom_crc.h"
}

namespace cyd::desktop::screen {

namespace {

constexpr int kDisplayMosi = 13;
constexpr int kDisplayMiso = 12;
constexpr int kDisplayClock = 14;
constexpr int kDisplayCs = 15;

}  // namespace

spi_device_handle_t display = nullptr;
alignas(4) uint16_t line_buffer[kSolidBufferPixels];
SemaphoreHandle_t display_mutex = nullptr;
volatile uint32_t spi_transactions = 0;
volatile uint64_t spi_bytes = 0;
// Sum of the CRC-32 of every buffer sent to the LCD, commands included.
// Two firmware builds that paint the same pixels in the same places give
// the same difference across the same sequence of actions, which lets a
// refactor of the painters be checked on the device without a camera.
volatile bool spi_fingerprint_enabled = false;
volatile uint32_t spi_fingerprint = 0;

bool transmit(const void *data, size_t length) {
    spi_transaction_t transaction{};
    transaction.length = length * 8;
    transaction.tx_buffer = data;
    const bool ok = spi_device_transmit(display, &transaction) == ESP_OK;
    if (ok) {
        ++spi_transactions;
        spi_bytes += length;
        if (spi_fingerprint_enabled)
            spi_fingerprint += esp_rom_crc32_le(0, static_cast<const uint8_t *>(data), length);
    }
    return ok;
}

void command(uint8_t value) {
    gpio_set_level(static_cast<gpio_num_t>(kDisplayDc), 0);
    transmit(&value, 1);
}

void data(const uint8_t *values, size_t length) {
    if (length == 0) return;
    gpio_set_level(static_cast<gpio_num_t>(kDisplayDc), 1);
    transmit(values, length);
}

void command_data(uint8_t value, const uint8_t *values, size_t length) {
    command(value);
    data(values, length);
}

bool init_display() {
    if (display != nullptr) return true;

    gpio_config_t output{};
    output.pin_bit_mask = (1ULL << kDisplayDc) | (1ULL << kBacklight);
    output.mode = GPIO_MODE_OUTPUT;
    gpio_config(&output);
    gpio_set_level(static_cast<gpio_num_t>(kBacklight), 1);

    spi_bus_config_t bus{};
    bus.mosi_io_num = kDisplayMosi;
    bus.miso_io_num = kDisplayMiso;
    bus.sclk_io_num = kDisplayClock;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = kSolidBufferPixels * sizeof(uint16_t);
    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;

    spi_device_interface_config_t device{};
    device.clock_speed_hz = 53333333;
    device.mode = 0;
    device.spics_io_num = kDisplayCs;
    device.queue_size = 2;
    if (spi_bus_add_device(SPI2_HOST, &device, &display) != ESP_OK) return false;

    command(0x01);  // software reset
    delay_ms(150);
    command(0x28);  // display off
    const uint8_t power_b[] = {0x00, 0xc1, 0x30};
    const uint8_t power_a[] = {0x64, 0x03, 0x12, 0x81};
    const uint8_t driver_timing[] = {0x85, 0x00, 0x78};
    const uint8_t power_1[] = {0x23};
    const uint8_t power_2[] = {0x10};
    const uint8_t vcom[] = {0x3e, 0x28};
    const uint8_t vcom_control[] = {0x86};
    // Matches TFT_eSPI ILI9342 rotation=2 used by the P0 hardware baseline:
    // MY | BGR.  Adding MX here mirrors all text horizontally.
    const uint8_t madctl[] = {0x88};  // 320x240 landscape, BGR
    const uint8_t pixel_format[] = {0x55};
    const uint8_t frame_rate[] = {0x00, 0x18};
    const uint8_t display_function[] = {0x08, 0x82, 0x27};
    const uint8_t entry_mode[] = {0x07};
    command_data(0xcf, power_b, sizeof(power_b));
    command_data(0xcb, power_a, sizeof(power_a));
    command_data(0xe8, driver_timing, sizeof(driver_timing));
    command_data(0xc0, power_1, sizeof(power_1));
    command_data(0xc1, power_2, sizeof(power_2));
    command_data(0xc5, vcom, sizeof(vcom));
    command_data(0xc7, vcom_control, sizeof(vcom_control));
    command_data(0x36, madctl, sizeof(madctl));
    command_data(0x3a, pixel_format, sizeof(pixel_format));
    command_data(0xb1, frame_rate, sizeof(frame_rate));
    command_data(0xb6, display_function, sizeof(display_function));
    command_data(0xb7, entry_mode, sizeof(entry_mode));
    command(0x11);  // sleep out
    delay_ms(120);
    command(0x29);  // display on
    return true;
}

void set_window(int x, int y, int w, int h) {
    const uint8_t column[] = {static_cast<uint8_t>(x >> 8), static_cast<uint8_t>(x),
                              static_cast<uint8_t>((x + w - 1) >> 8), static_cast<uint8_t>(x + w - 1)};
    const uint8_t row[] = {static_cast<uint8_t>(y >> 8), static_cast<uint8_t>(y),
                           static_cast<uint8_t>((y + h - 1) >> 8), static_cast<uint8_t>(y + h - 1)};
    command_data(0x2a, column, sizeof(column));
    command_data(0x2b, row, sizeof(row));
    command(0x2c);
}

}  // namespace cyd::desktop::screen
