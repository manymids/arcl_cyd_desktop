#pragma once

// Rectangles and the dirty-region list used by every retained view.
// Pure logic with no ESP-IDF dependency, so it is covered by host tests.

#include <algorithm>
#include <cstdint>

namespace cyd::desktop::screen {

struct Rect { int x; int y; int width; int height; };

inline bool rect_empty(const Rect &rect) { return rect.width <= 0 || rect.height <= 0; }

inline Rect intersect_rect(const Rect &a, const Rect &b) {
    const int left = std::max(a.x, b.x);
    const int top = std::max(a.y, b.y);
    const int right = std::min(a.x + a.width, b.x + b.width);
    const int bottom = std::min(a.y + a.height, b.y + b.height);
    return {left, top, right - left, bottom - top};
}

inline bool rects_overlap_or_touch(const Rect &a, const Rect &b) {
    return a.x <= b.x + b.width && b.x <= a.x + a.width &&
           a.y <= b.y + b.height && b.y <= a.y + a.height;
}

inline Rect union_rect(const Rect &a, const Rect &b) {
    const int left = std::min(a.x, b.x);
    const int top = std::min(a.y, b.y);
    const int right = std::max(a.x + a.width, b.x + b.width);
    const int bottom = std::max(a.y + a.height, b.y + b.height);
    return {left, top, right - left, bottom - top};
}

struct DirtyRects {
    Rect values[32];
    uint8_t count = 0;

    void add(Rect rect, const Rect &bounds) {
        rect = intersect_rect(rect, bounds);
        if (rect_empty(rect)) return;
        for (uint8_t index = 0; index < count;) {
            if (!rects_overlap_or_touch(rect, values[index])) {
                ++index;
                continue;
            }
            rect = union_rect(rect, values[index]);
            values[index] = values[--count];
            index = 0;
        }
        if (count < sizeof(values) / sizeof(values[0])) {
            values[count++] = rect;
        } else {
            values[0] = union_rect(values[0], rect);
        }
    }

    void sort_top_to_bottom() {
        // A stable insertion sort is sufficient for this tiny bounded list and
        // makes LCD updates progress like text scanlines instead of appearing
        // in the merge order of the changed elements.
        for (uint8_t index = 1; index < count; ++index) {
            const Rect value = values[index];
            uint8_t position = index;
            while (position > 0 &&
                   (values[position - 1].y > value.y ||
                    (values[position - 1].y == value.y && values[position - 1].x > value.x))) {
                values[position] = values[position - 1];
                --position;
            }
            values[position] = value;
        }
    }
};

}  // namespace cyd::desktop::screen
