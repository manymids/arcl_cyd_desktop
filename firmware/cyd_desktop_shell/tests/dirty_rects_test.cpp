// DirtyRects decides which parts of a retained app window and of the clock
// are repainted. It moved out of screen.cpp into include/screen/rect.h, which
// has no ESP-IDF dependency, so it can be checked here.

#include <cassert>
#include <cstdio>

#include "screen/rect.h"

using cyd::desktop::screen::DirtyRects;
using cyd::desktop::screen::Rect;
using cyd::desktop::screen::intersect_rect;
using cyd::desktop::screen::rect_empty;

namespace {

constexpr Rect kBounds{14, 44, 292, 158};

bool same(const Rect &a, const Rect &b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

void clips_to_bounds_and_drops_empty() {
    DirtyRects dirty;
    dirty.add({0, 0, 10, 10}, kBounds);
    assert(dirty.count == 0);
    dirty.add({0, 40, 30, 10}, kBounds);
    assert(dirty.count == 1);
    assert(same(dirty.values[0], {14, 44, 16, 6}));
}

void merges_touching_regions() {
    DirtyRects dirty;
    dirty.add({20, 50, 10, 10}, kBounds);
    dirty.add({30, 50, 10, 10}, kBounds);   // shares an edge
    assert(dirty.count == 1);
    assert(same(dirty.values[0], {20, 50, 20, 10}));
    dirty.add({100, 100, 5, 5}, kBounds);   // separate
    assert(dirty.count == 2);
    dirty.add({38, 58, 70, 45}, kBounds);   // bridges both
    assert(dirty.count == 1);
}

void never_exceeds_capacity() {
    DirtyRects dirty;
    for (int i = 0; i < 40; ++i) dirty.add({16 + i * 7, 46, 2, 2}, kBounds);
    assert(dirty.count == 32);
}

void sorts_top_to_bottom_then_left_to_right() {
    DirtyRects dirty;
    dirty.add({200, 150, 4, 4}, kBounds);
    dirty.add({100, 60, 4, 4}, kBounds);
    dirty.add({20, 150, 4, 4}, kBounds);
    dirty.sort_top_to_bottom();
    assert(dirty.values[0].y == 60);
    assert(dirty.values[1].x == 20 && dirty.values[1].y == 150);
    assert(dirty.values[2].x == 200);
}

void intersection_of_disjoint_rects_is_empty() {
    assert(rect_empty(intersect_rect({0, 0, 10, 10}, {20, 20, 5, 5})));
    assert(!rect_empty(intersect_rect({0, 0, 10, 10}, {5, 5, 10, 10})));
}

}  // namespace

int main() {
    clips_to_bounds_and_drops_empty();
    merges_touching_regions();
    never_exceeds_capacity();
    sorts_top_to_bottom_then_left_to_right();
    intersection_of_disjoint_rects_is_empty();
    std::puts("dirty_rects_test: all assertions passed");
    return 0;
}
