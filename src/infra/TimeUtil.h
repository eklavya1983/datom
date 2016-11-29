#pragma once

#include <chrono>

namespace infra {

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

inline TimePoint getTimePoint()
{
    return std::chrono::steady_clock::now();
}

inline auto elapsedTime(const TimePoint &from)
{
    return (getTimePoint() - from);
}

}  // namespace
