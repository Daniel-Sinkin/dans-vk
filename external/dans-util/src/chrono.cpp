// src/chrono.cpp
//
#include <dans/chrono.hpp>
// Externals
#include <dans/development_markers.hpp>
#include <dans/types.hpp>
// StdLib
#include <chrono>
#include <format>
#include <string>
#include <string_view>
//

namespace dans::chrono
{
[[nodiscard]] def format_seconds(std::chrono::duration<f64> dur, bool show_milliseconds)
    -> std::string
{
    const auto is_negative = dur < std::chrono::duration<f64>::zero();
    const auto positive_duration = is_negative ? -dur : dur;

    using Msec = std::chrono::milliseconds;
    constexpr Msec::rep ms_per_second{1000};
    constexpr Msec::rep ms_per_minute{ms_per_second * 60};
    constexpr Msec::rep ms_per_hour{ms_per_minute * 60};
    constexpr Msec::rep ms_per_day{ms_per_hour * 24};

    mut auto remaining = std::chrono::duration_cast<Msec>(positive_duration).count();

    const auto days = remaining / ms_per_day;
    remaining %= ms_per_day;
    const auto hours = remaining / ms_per_hour;
    remaining %= ms_per_hour;
    const auto minutes = remaining / ms_per_minute;
    remaining %= ms_per_minute;
    const auto seconds = remaining / ms_per_second;
    const auto milliseconds = remaining % ms_per_second;

    mut std::string result{};
    mut auto started = false;

    const auto append_time_unit = [&](Msec::rep value, std::string_view name) -> void
    {
        if (not started and value == 0) return;
        if (not result.empty()) result += ' ';
        result += std::format("{} {}{}", value, name, value == 1 ? "" : "s");
        started = true;
    };
    append_time_unit(days, "day");
    append_time_unit(hours, "hour");
    append_time_unit(minutes, "minute");
    append_time_unit(seconds, "second");
    if (show_milliseconds) append_time_unit(milliseconds, "millisecond");

    if (not started) result = show_milliseconds ? "0 milliseconds" : "0 seconds";

    if (is_negative) return std::format("-{}", result);
    return result;
}
}  // namespace dans::chrono
