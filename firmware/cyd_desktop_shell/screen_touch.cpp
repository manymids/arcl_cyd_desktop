#include "screen/touch.h"
#include "screen/draw.h"
#include "screen/views.h"

extern "C" {
#include "driver/gpio.h"
#include "freertos/task.h"
#include "nvs.h"
}

#include <cmath>
#include <cstdio>

namespace cyd::desktop::screen {

namespace {

constexpr int kTouchCs = 33;
constexpr int kTouchIrq = 36;
constexpr int kTouchClock = 25;
constexpr int kTouchMiso = 39;
constexpr int kTouchMosi = 32;
constexpr uint32_t kCalibrationMagic = 0x43594444;

struct Calibration {
    uint32_t magic;
    float x_a;
    float x_b;
    float x_c;
    float y_a;
    float y_b;
    float y_c;
};

struct CalibrationPoint { uint16_t x; uint16_t y; };

Calibration calibration{};
Calibration calibration_backup{};
bool calibration_backup_valid = false;
CalibrationPoint calibration_points[3] = {};

void load_calibration() {
    nvs_handle_t handle;
    if (nvs_open("cyd_desktop", NVS_READONLY, &handle) != ESP_OK) return;
    size_t length = sizeof(calibration);
    const esp_err_t result = nvs_get_blob(handle, "touch", &calibration, &length);
    nvs_close(handle);
    calibrated = result == ESP_OK && length == sizeof(calibration) && calibration.magic == kCalibrationMagic;
}

bool save_calibration() {
    nvs_handle_t handle;
    if (nvs_open("cyd_desktop", NVS_READWRITE, &handle) != ESP_OK) return false;
    const esp_err_t result = nvs_set_blob(handle, "touch", &calibration, sizeof(calibration));
    if (result == ESP_OK) nvs_commit(handle);
    nvs_close(handle);
    return result == ESP_OK;
}

bool solve_calibration() {
    const auto solve = [](int dimension, float *a, float *b, float *c) {
        float matrix[3][4] = {};
        for (int row = 0; row < 3; ++row) {
            matrix[row][0] = calibration_points[row].x;
            matrix[row][1] = calibration_points[row].y;
            matrix[row][2] = 1.0f;
            matrix[row][3] = kCalibrationTargets[row][dimension];
        }
        for (int pivot = 0; pivot < 3; ++pivot) {
            int best = pivot;
            for (int row = pivot + 1; row < 3; ++row)
                if (std::fabs(matrix[row][pivot]) > std::fabs(matrix[best][pivot])) best = row;
            if (std::fabs(matrix[best][pivot]) < 0.0001f) return false;
            for (int column = pivot; column < 4; ++column) {
                const float saved = matrix[pivot][column];
                matrix[pivot][column] = matrix[best][column];
                matrix[best][column] = saved;
            }
            const float divisor = matrix[pivot][pivot];
            for (int column = pivot; column < 4; ++column) matrix[pivot][column] /= divisor;
            for (int row = 0; row < 3; ++row) {
                if (row == pivot) continue;
                const float factor = matrix[row][pivot];
                for (int column = pivot; column < 4; ++column) matrix[row][column] -= factor * matrix[pivot][column];
            }
        }
        *a = matrix[0][3];
        *b = matrix[1][3];
        *c = matrix[2][3];
        return true;
    };
    calibration.magic = kCalibrationMagic;
    return solve(0, &calibration.x_a, &calibration.x_b, &calibration.x_c)
        && solve(1, &calibration.y_a, &calibration.y_b, &calibration.y_c)
        && save_calibration();
}

void touch_pin(int pin, int level) { gpio_set_level(static_cast<gpio_num_t>(pin), level); }

uint16_t touch_read(uint8_t request) {
    touch_pin(kTouchCs, 0);
    touch_pin(kTouchClock, 0);
    for (int bit = 7; bit >= 0; --bit) {
        touch_pin(kTouchMosi, (request >> bit) & 1);
        touch_pin(kTouchClock, 1);
        touch_pin(kTouchClock, 0);
    }
    uint16_t value = 0;
    for (int bit = 0; bit < 12; ++bit) {
        touch_pin(kTouchClock, 1);
        value = static_cast<uint16_t>((value << 1) | gpio_get_level(static_cast<gpio_num_t>(kTouchMiso)));
        touch_pin(kTouchClock, 0);
    }
    touch_pin(kTouchCs, 1);
    return value;
}

}  // namespace

bool calibrated = false;
bool calibration_active = false;
uint8_t calibration_step = 0;
QueueHandle_t synthetic_taps = nullptr;
volatile bool physical_touch_down = false;
volatile int16_t last_touch_x = -1;
volatile int16_t last_touch_y = -1;
volatile uint32_t synthetic_tap_count = 0;

void init_touch() {
    gpio_config_t touch_output{};
    touch_output.pin_bit_mask = (1ULL << kTouchCs) | (1ULL << kTouchClock) | (1ULL << kTouchMosi);
    touch_output.mode = GPIO_MODE_OUTPUT;
    gpio_config(&touch_output);
    gpio_set_level(static_cast<gpio_num_t>(kTouchCs), 1);
    gpio_config_t touch_input{};
    touch_input.pin_bit_mask = (1ULL << kTouchIrq) | (1ULL << kTouchMiso);
    touch_input.mode = GPIO_MODE_INPUT;
    gpio_config(&touch_input);
    load_calibration();
    if (synthetic_taps == nullptr) synthetic_taps = xQueueCreate(16, sizeof(SyntheticTap));
}

void begin_calibration() {
    calibration_backup = calibration;
    calibration_backup_valid = calibrated;
    calibration_active = true;
    calibration_step = 0;
}

bool read_touch_raw(uint16_t *raw_x, uint16_t *raw_y) {
    if ((!calibrated && !calibration_active) || gpio_get_level(static_cast<gpio_num_t>(kTouchIrq)) != 0) return false;
    *raw_x = touch_read(0xd0);
    *raw_y = touch_read(0x90);
    // XPT2046 can report 0/4095 ghost values while idle.  Treat only the
    // calibrated ADC working range as a genuine press.
    return *raw_x >= 200 && *raw_x <= 3950 && *raw_y >= 200 && *raw_y <= 3950;
}

bool map_touch(uint16_t raw_x, uint16_t raw_y, int *x, int *y) {
    *x = static_cast<int>(calibration.x_a * raw_x + calibration.x_b * raw_y + calibration.x_c + 0.5f);
    *y = static_cast<int>(calibration.y_a * raw_x + calibration.y_b * raw_y + calibration.y_c + 0.5f);
    return *x >= 0 && *x < kWidth && *y >= 0 && *y < kHeight;
}

bool touch_irq_pressed() {
    return (calibrated || calibration_active) && gpio_get_level(static_cast<gpio_num_t>(kTouchIrq)) == 0;
}

void calibration_record_point(uint16_t raw_x, uint16_t raw_y) {
    calibration_points[calibration_step++] = {raw_x, raw_y};
    if (calibration_step == 3) {
        calibration_active = false;
        const bool calibration_succeeded = solve_calibration();
        calibrated = calibration_succeeded;
        if (!calibration_succeeded && calibration_backup_valid) {
            calibration = calibration_backup;
            calibrated = true;
        } else if (!calibration_succeeded) {
            calibration.magic = 0;
        }
        calibration_backup_valid = false;
        record_input(calibration_succeeded ? "touch.calibrated" : "touch.calibration_failed");
    }
}

void render_calibration() {
    fill_rect(0, 0, kWidth, kHeight, kBlack);
    text("TOUCH CALIBRATION", 72, 30, kWhite, 2);
    text("TAP THE CROSSHAIR", 84, 52, kCyan, 1);
    const int x = kCalibrationTargets[calibration_step][0];
    const int y = kCalibrationTargets[calibration_step][1];
    line(x - 12, y, x + 12, y, kRed, 2);
    line(x, y - 12, x, y + 12, kRed, 2);
    frame(x - 8, y - 8, 17, 17, kYellow);
    char step[20];
    std::snprintf(step, sizeof(step), "%d / 3", calibration_step + 1);
    text(step, 144, 188, kWhite, 2);
}

}  // namespace cyd::desktop::screen

using namespace cyd::desktop::screen;

extern "C" bool cyd_desktop_touch_calibrated(void) { return calibrated; }

extern "C" bool cyd_desktop_touch_inject(int x, int y) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight || synthetic_taps == nullptr) return false;
    const SyntheticTap tap{static_cast<int16_t>(x), static_cast<int16_t>(y)};
    return xQueueSend(synthetic_taps, &tap, pdMS_TO_TICKS(20)) == pdTRUE;
}

extern "C" void cyd_desktop_touch_state(bool *pressed, int *x, int *y, unsigned *pending) {
    if (pressed != nullptr) *pressed = physical_touch_down;
    if (x != nullptr) *x = last_touch_x;
    if (y != nullptr) *y = last_touch_y;
    if (pending != nullptr) *pending = synthetic_taps == nullptr ? 0 : uxQueueMessagesWaiting(synthetic_taps);
}

extern "C" void cyd_desktop_touch_calibration_start(void) {
    begin_calibration();
    redraw_requested = true;
}

extern "C" void cyd_desktop_game_touch(bool *pressed, int *x, int *y) {
    if (pressed != nullptr) *pressed = physical_touch_down;
    if (x != nullptr) *x = last_touch_x;
    if (y != nullptr) *y = last_touch_y;
}

