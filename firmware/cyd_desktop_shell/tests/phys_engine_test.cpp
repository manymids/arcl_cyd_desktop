// Host test for phys_engine.cpp, the physics behind cyd.phys_* (API level 8).
#include "phys_engine.h"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

phys_state_t state_of(int id) {
    phys_state_t slots[PHYS_CAP];
    phys_copy_state(slots, PHYS_CAP);
    return slots[id];
}

void run(float seconds) {
    for (int frame = 0; frame < static_cast<int>(seconds * 60.0f); ++frame) phys_step(1.0f / 60.0f);
}

void sphere_rests_on_floor() {
    phys_reset(3, 0.0f, -520.0f, 0.0f);
    assert(phys_body(0, PHYS_SHAPE_PLANE, 0, 0, 0, 0, 1, 0, 0, 0, 0) == 0);
    assert(phys_body(1, PHYS_SHAPE_SPHERE, 0, 100, 0, 10, 0, 0, 1, 0.3f, 0.4f) == 1);
    run(3.0f);
    const phys_state_t ball = state_of(1);
    assert(ball.active == 1 && ball.shape == PHYS_SHAPE_SPHERE && ball.flags == 1);
    // Settled on the floor: centre one radius up, within the solver's slop.
    assert(std::fabs(ball.y - 10.0f) < 1.5f);
    assert(std::fabs(ball.vy) < 5.0f);
    assert(phys_active_count() == 2);
}

void boxes_stack() {
    phys_reset(3, 0.0f, -520.0f, 0.0f);
    phys_body(0, PHYS_SHAPE_PLANE, 0, 0, 0, 0, 1, 0, 0, 0, 0);
    phys_body(1, PHYS_SHAPE_BOX, 0, 20, 0, 10, 10, 10, 1, 0.1f, 0.6f);
    phys_body(2, PHYS_SHAPE_BOX, 0, 60, 0, 10, 10, 10, 1, 0.1f, 0.6f);
    run(4.0f);
    const phys_state_t lower = state_of(1);
    const phys_state_t upper = state_of(2);
    assert(std::fabs(lower.y - 10.0f) < 2.0f);
    assert(std::fabs(upper.y - 30.0f) < 3.0f);
    assert(upper.y > lower.y);
}

void two_dimensions_stay_flat() {
    phys_reset(2, 0.0f, -520.0f, 0.0f);
    phys_body(0, PHYS_SHAPE_PLANE, 0, 0, 0, 0, 1, 0, 0, 0, 0);
    phys_body(1, PHYS_SHAPE_SPHERE, 0, 50, 30, 5, 0, 0, 1, 0.5f, 0.4f);
    assert(phys_velocity(1, 40.0f, 0.0f, 99.0f) == 1);
    run(1.0f);
    const phys_state_t ball = state_of(1);
    assert(ball.z == 0.0f && ball.vz == 0.0f);
    assert(ball.x > 10.0f);
}

void ball_bounces_off_wall() {
    phys_reset(2, 0.0f, 0.0f, 0.0f);
    // Wall at x=100 facing -x. Bounciness is the smaller of the two restitutions.
    phys_body(0, PHYS_SHAPE_PLANE, 100, 0, 0, -1, 0, 0, 0, 1.0f, 0);
    phys_body(1, PHYS_SHAPE_SPHERE, 0, 0, 0, 5, 0, 0, 1, 1.0f, 0.0f);
    phys_velocity(1, 200.0f, 0.0f, 0.0f);
    run(1.0f);
    const phys_state_t ball = state_of(1);
    assert(ball.vx < 0.0f);
    assert(ball.x < 100.0f - 4.0f);
}

void slots_and_inputs_are_checked() {
    phys_reset(3, 0.0f, 0.0f, 0.0f);
    assert(phys_body(-1, PHYS_SHAPE_SPHERE, 0, 0, 0, 1, 0, 0, 1, 0, 0) == -1);
    assert(phys_body(PHYS_CAP, PHYS_SHAPE_SPHERE, 0, 0, 0, 1, 0, 0, 1, 0, 0) == -1);
    assert(phys_body(0, 9, 0, 0, 0, 1, 0, 0, 1, 0, 0) == -1);
    // A static body cannot be given a velocity.
    assert(phys_body(0, PHYS_SHAPE_BOX, 0, 0, 0, 5, 5, 5, 0, 0, 0) == 0);
    assert(phys_velocity(0, 1, 1, 1) == 0);
    assert(phys_velocity(5, 1, 1, 1) == 0);
    // Shape 0 removes a body.
    assert(phys_body(1, PHYS_SHAPE_SPHERE, 50, 0, 0, 3, 0, 0, 1, 0, 0) == 1);
    assert(phys_active_count() == 2);
    assert(phys_body(1, PHYS_SHAPE_NONE, 0, 0, 0, 0, 0, 0, 0, 0, 0) == 1);
    assert(phys_active_count() == 1);
    // NaN input falls back to a sane value instead of poisoning the world.
    phys_reset(3, 0.0f, 0.0f, 0.0f);
    assert(phys_body(1, PHYS_SHAPE_SPHERE, NAN, 0, 0, 3, 0, 0, 1, 0, 0) == 1);
    phys_step(1.0f / 60.0f);
    assert(state_of(1).x == 0.0f);
    phys_reset(3, 0.0f, 0.0f, 0.0f);
    assert(phys_active_count() == 0);
}

}  // namespace

int main() {
    static_assert(sizeof(phys_state_t) == 40, "cyd.PHYS_STRIDE is 40");
    sphere_rests_on_floor();
    boxes_stack();
    two_dimensions_stay_flat();
    ball_bounces_off_wall();
    slots_and_inputs_are_checked();
    std::puts("phys_engine_test: all assertions passed");
    return 0;
}
