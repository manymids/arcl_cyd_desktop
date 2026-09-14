#include "screen/views.h"
#include "screen/widgets.h"
#include "screen/time_util.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace cyd::desktop::screen {

namespace {

struct ClockHand {
    int x;
    int y;
    int width;
    uint16_t color;
};

struct ClockDisplayState {
    bool valid;
    ClockHand hour;
    ClockHand minute;
    ClockHand second;
    char time_text[32];
    char date_text[40];
};
ClockDisplayState displayed_clock{};

constexpr int kClockCenterX = 90;
constexpr int kClockCenterY = 117;
constexpr int kClockRadius = 56;
constexpr float kPi = 3.14159265f;
constexpr Rect kClockFaceBounds{29, 56, 123, 123};

ClockHand clock_hand(float angle, int length, int width, uint16_t color) {
    return {kClockCenterX + static_cast<int>(std::cos(angle) * length),
            kClockCenterY + static_cast<int>(std::sin(angle) * length), width, color};
}

Rect clock_hand_bounds(const ClockHand &hand) {
    const int padding = (hand.width + 1) / 2 + 1;
    const int left = std::min(kClockCenterX, hand.x) - padding;
    const int top = std::min(kClockCenterY, hand.y) - padding;
    const int right = std::max(kClockCenterX, hand.x) + padding + 1;
    const int bottom = std::max(kClockCenterY, hand.y) + padding + 1;
    return {left, top, right - left, bottom - top};
}

bool clock_hand_equal(const ClockHand &a, const ClockHand &b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.color == b.color;
}

ClockDisplayState make_clock_state(const std::tm &now) {
    ClockDisplayState state{};
    state.valid = true;
    const float second_angle = (static_cast<float>(now.tm_sec) / 60.0f) * 2.0f * kPi - kPi / 2.0f;
    const float minute_angle = ((static_cast<float>(now.tm_min) + now.tm_sec / 60.0f) / 60.0f) * 2.0f * kPi - kPi / 2.0f;
    const float hour_angle = ((static_cast<float>(now.tm_hour % 12) + now.tm_min / 60.0f) / 12.0f) * 2.0f * kPi - kPi / 2.0f;
    state.hour = clock_hand(hour_angle, 30, 4, kBlack);
    state.minute = clock_hand(minute_angle, 43, 3, kWindowBlue);
    state.second = clock_hand(second_angle, 49, 1, kRed);
    std::snprintf(state.time_text, sizeof(state.time_text), "%02d:%02d:%02d", now.tm_hour, now.tm_min, now.tm_sec);
    std::snprintf(state.date_text, sizeof(state.date_text), "%04d-%02d-%02d", now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
    return state;
}

void draw_clock_marks() {
    for (int mark = 0; mark < 60; ++mark) {
        const float angle = (static_cast<float>(mark) / 60.0f) * 2.0f * kPi - kPi / 2.0f;
        const int outer_x = kClockCenterX + static_cast<int>(std::cos(angle) * kClockRadius);
        const int outer_y = kClockCenterY + static_cast<int>(std::sin(angle) * kClockRadius);
        const int inner_radius = kClockRadius - ((mark % 5 == 0) ? 7 : 3);
        const int inner_x = kClockCenterX + static_cast<int>(std::cos(angle) * inner_radius);
        const int inner_y = kClockCenterY + static_cast<int>(std::sin(angle) * inner_radius);
        line(inner_x, inner_y, outer_x, outer_y, mark % 5 == 0 ? kDarkGray : kAccentSoft,
             mark % 5 == 0 ? 2 : 1);
    }
}

void draw_clock_face() {
    circle(kClockCenterX, kClockCenterY, kClockRadius + 4, kAccentSoft, 2);
    draw_clock_marks();
    centered_text("12", kClockCenterX, 67, kDarkGray);
    centered_text("6", kClockCenterX, 158, kDarkGray);
    text("9", 40, 114, kDarkGray);
    text("3", 135, 114, kDarkGray);
}

void draw_clock_hand(const ClockHand &hand) {
    line(kClockCenterX, kClockCenterY, hand.x, hand.y, hand.color, hand.width);
}

void draw_clock_dynamic(const ClockDisplayState &state) {
    draw_clock_hand(state.hour);
    draw_clock_hand(state.minute);
    draw_clock_hand(state.second);
    fill_rect(kClockCenterX - 3, kClockCenterY - 3, 7, 7, kRed);
}

void update_clock_text(const char *old_value, const char *new_value, int x, int y, uint16_t color, int scale) {
    const size_t old_length = std::strlen(old_value);
    const size_t new_length = std::strlen(new_value);
    const size_t length = std::max(old_length, new_length);
    const int advance = 6 * scale;
    for (size_t index = 0; index < length; ++index) {
        const char old_character = index < old_length ? old_value[index] : '\0';
        const char new_character = index < new_length ? new_value[index] : '\0';
        if (old_character == new_character) continue;
        fill_rect(x + static_cast<int>(index) * advance, y, advance, 7 * scale, kCardAlt);
        if (new_character != '\0') {
            const char glyph_text[] = {new_character, '\0'};
            text(glyph_text, x + static_cast<int>(index) * advance, y, color, scale);
        }
    }
}

void paint_clock_full(const ClockDisplayState &state) {
    wallpaper();
    card(8, 8, 304, 197, 13, kCard);
    draw_icon(UiIcon::Clock, 22, 19, kWindowBlue);
    text("Clock", 48, 20, kBlack, 2);
    text("LOCAL TIME", 224, 25, kDarkGray);
    card(18, 45, 145, 146, 12, kCardAlt);
    draw_clock_face();
    draw_clock_dynamic(state);
    card(173, 51, 129, 57, 10, kCardAlt);
    text("CURRENT", 187, 61, kDarkGray);
    text(state.time_text, 189, 77, kBlack, 2);
    card(173, 117, 129, 49, 10, kCardAlt);
    text("TODAY", 187, 127, kDarkGray);
    text(state.date_text, 207, 144, kBlack);
    fill_round_rect(190, 178, 95, 18, 9, kAccentSoft);
    centered_text("CLOCK IS ACTIVE", 237, 184, kWindowBlue);
    taskbar();
}

void render_clock_full(const ClockDisplayState &state) {
    render_in_transition_tiles([&state]() { paint_clock_full(state); });
    displayed_clock = state;
}

}  // namespace

void render_clock() {
    ui_node("clock.face", "clock", 18, 45, 145, 146, "Analog clock", false);
    ui_node("clock.time", "text", 173, 51, 129, 57, "Current time", false);
    std::tm now{};
    current_time(&now);
    const ClockDisplayState next = make_clock_state(now);
    if (!displayed_clock.valid) {
        render_clock_full(next);
        return;
    }

    DirtyRects analog_dirty;
    const auto add_changed_hand = [&analog_dirty](const ClockHand &old_hand, const ClockHand &new_hand) {
        if (clock_hand_equal(old_hand, new_hand)) return;
        analog_dirty.add(clock_hand_bounds(old_hand), kClockFaceBounds);
        analog_dirty.add(clock_hand_bounds(new_hand), kClockFaceBounds);
    };
    add_changed_hand(displayed_clock.hour, next.hour);
    add_changed_hand(displayed_clock.minute, next.minute);
    add_changed_hand(displayed_clock.second, next.second);

    analog_dirty.sort_top_to_bottom();
    for (uint8_t index = 0; index < analog_dirty.count; ++index) {
        const Rect &dirty = analog_dirty.values[index];
        fill_rect(dirty.x, dirty.y, dirty.width, dirty.height, kCardAlt);
        set_clip(dirty);
        draw_clock_face();
        draw_clock_dynamic(next);
        clear_clip();
    }

    update_clock_text(displayed_clock.time_text, next.time_text, 189, 77, kBlack, 2);
    update_clock_text(displayed_clock.date_text, next.date_text, 207, 144, kBlack, 1);
    displayed_clock = next;
}

namespace {

void paint_calendar() {
    ui_node("calendar.grid", "calendar", 18, 43, 284, 145, "Month calendar", false);
    std::tm now{};
    current_time(&now);
    const int year = now.tm_year + 1900;
    const int month = now.tm_mon;
    std::tm first = now;
    first.tm_mday = 1;
    std::mktime(&first);
    const int first_weekday = first.tm_wday;
    wallpaper();
    card(8, 8, 304, 197, 13, kCard);
    draw_icon(UiIcon::Calendar, 22, 18, kWindowBlue);
    text("Calendar", 48, 19, kBlack, 2);
    char heading[32];
    std::snprintf(heading, sizeof(heading), "%04d-%02d", year, month + 1);
    fill_round_rect(116, 43, 88, 25, 12, kAccentSoft);
    centered_text(heading, 160, 49, kWindowBlue, 1);
    static const char *weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    for (int column = 0; column < 7; ++column) {
        const uint16_t color = column == 0 ? kRed : (column == 6 ? kWindowBlue : kDarkGray);
        centered_text(weekdays[column], 24 + column * 45, 77, color);
    }
    fill_rect(18, 89, 284, 1, kAccentSoft);
    const int max_day = days_in_month(year, month);
    for (int day = 1; day <= max_day; ++day) {
        const int slot = first_weekday + day - 1;
        const int column = slot % 7;
        const int row = slot / 7;
        const int x = 16 + column * 45;
        const int y = 96 + row * 17;
        if (day == now.tm_mday) {
            fill_round_rect(x - 1, y - 4, 29, 15, 7, kWindowBlue);
        }
        char day_text[12];
        std::snprintf(day_text, sizeof(day_text), "%02d", day);
        const uint16_t color = day == now.tm_mday ? kWhite
            : (column == 0 ? kRed : (column == 6 ? kWindowBlue : kBlack));
        text(day_text, x + 7, y, color);
    }
    fill_round_rect(111, 187, 98, 13, 6, kCardAlt);
    centered_text("TODAY HIGHLIGHT", 160, 190, kDarkGray);
    taskbar();
}

}  // namespace

void render_calendar() {
    render_in_transition_tiles([]() { paint_calendar(); });
}

void clock_view_invalidate() { displayed_clock.valid = false; }

}  // namespace cyd::desktop::screen
