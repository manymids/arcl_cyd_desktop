#pragma once

// XPT2046 touch input, its three-point calibration and the queue of taps
// injected through the protocol (desktop_tap).

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace cyd::desktop::screen {

constexpr uint16_t kCalibrationTargets[3][2] = {{20, 20}, {300, 20}, {160, 220}};

struct SyntheticTap { int16_t x; int16_t y; };

extern bool calibrated;
extern bool calibration_active;
extern uint8_t calibration_step;

extern QueueHandle_t synthetic_taps;
extern volatile bool physical_touch_down;
extern volatile int16_t last_touch_x;
extern volatile int16_t last_touch_y;
extern volatile uint32_t synthetic_tap_count;

// Configures the touch pins, loads the stored calibration and creates the
// injected-tap queue.
void init_touch();

bool touch_irq_pressed();
bool read_touch_raw(uint16_t *raw_x, uint16_t *raw_y);
bool map_touch(uint16_t raw_x, uint16_t raw_y, int *x, int *y);

void begin_calibration();
// Records the raw sample for the current target; after the third it solves,
// saves, and leaves calibration mode.
void calibration_record_point(uint16_t raw_x, uint16_t raw_y);
void render_calibration();

}  // namespace cyd::desktop::screen
