#include "native_app.h"

#include <cstring>

namespace cyd::desktop::native {
namespace {

const NativeApp *running = nullptr;
RuntimeMetrics runtime_metrics{};
uint64_t accumulated_frame_ms = 0;

}  // namespace

bool launch(const char *id) {
    const NativeApp *next = find(id);
    if (next == nullptr) return false;
    if (running == next) return true;
    leave();
    running = next;
    runtime_metrics = {};
    accumulated_frame_ms = 0;
    if (running->enter != nullptr) running->enter();
    return true;
}

void leave() {
    if (running != nullptr && running->leave != nullptr) running->leave();
    running = nullptr;
}

bool active() { return running != nullptr; }

const NativeApp *current() { return running; }

bool update(uint32_t delta_ms) {
    return running != nullptr && running->update != nullptr && running->update(delta_ms);
}

void input(const InputEvent &event) {
    if (running != nullptr && running->input != nullptr) running->input(event);
}

void render_tile(const TileSurface &surface) {
    if (running != nullptr && running->render_tile != nullptr) running->render_tile(surface);
}

void note_frame(uint32_t elapsed_ms) {
    runtime_metrics.last_frame_ms = elapsed_ms;
    accumulated_frame_ms += elapsed_ms;
    ++runtime_metrics.frames;
    runtime_metrics.average_frame_ms = runtime_metrics.frames == 0
        ? 0 : static_cast<uint32_t>(accumulated_frame_ms / runtime_metrics.frames);
}

RuntimeMetrics metrics() { return runtime_metrics; }

}  // namespace cyd::desktop::native
