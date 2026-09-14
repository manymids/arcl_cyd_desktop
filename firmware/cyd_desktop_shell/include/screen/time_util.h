#pragma once

#include <ctime>

namespace cyd::desktop::screen {

inline bool current_time(std::tm *value) {
    const std::time_t now = std::time(nullptr);
    if (now < 0) return false;
    return localtime_r(&now, value) != nullptr;
}

// month is 0-based, as in std::tm.
inline int days_in_month(int year, int month) {
    static const int normal[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month != 1) return normal[month];
    return ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 29 : 28;
}

}  // namespace cyd::desktop::screen
