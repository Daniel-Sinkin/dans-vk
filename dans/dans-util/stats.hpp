// dans/dans-util/stats.hpp
// Externals
#include "dans/dans-core/development_markers.hpp"
#include "dans/dans-core/types.hpp"
#include "dans/dans-util/math.hpp"
// StdLib
#include <algorithm>
#include <cmath>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>
//

#pragma once
#ifndef DANS_STATS_HPP
#    define DANS_STATS_HPP

namespace dans::stats
{
struct Summary
{
    usize count{};
    f64 mean{};
    f64 min{};
    f64 max{};
    f64 median{};
    f64 p90{};
    f64 p95{};
    f64 p99{};
    f64 stddev{};
};

template <typename T>
    requires std::is_arithmetic_v<T>
[[nodiscard]] def summarize(std::span<const T> values) -> Summary
{
    using std::views::transform;
    if (values.empty()) return Summary{};

    const auto sorted = [&]() -> std::vector<f64>
    {
        const auto to_f64 = [&](auto x) { return static_cast<f64>(x); };
        auto v = values | transform(to_f64) | std::ranges::to<std::vector<f64>>();
        std::ranges::sort(v);
        return v;
    }();

    const auto avg = math::mean(sorted);
    const auto variance =
        math::mean(sorted | transform([avg](auto v) { return (v - avg) * (v - avg); }));
    const auto stddev = std::sqrt(variance);

    const auto percentile = [&](f64 p) -> f64
    {
        const auto rank = p * static_cast<f64>(sorted.size() - 1);
        const auto lo = static_cast<usize>(std::floor(rank));
        const auto hi = static_cast<usize>(std::ceil(rank));
        if (lo == hi) return sorted[lo];
        const auto frac = rank - static_cast<f64>(lo);
        return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
    };

    return Summary{
        .count = sorted.size(),
        .mean = avg,
        .min = sorted.front(),
        .max = sorted.back(),
        .median = percentile(0.5),
        .p90 = percentile(0.9),
        .p95 = percentile(0.95),
        .p99 = percentile(0.99),
        .stddev = stddev,
    };
}

template <typename T>
    requires std::is_arithmetic_v<T>
[[nodiscard]] def summarize(const std::vector<T>& values) -> Summary
{
    return summarize(std::span<const T>{values});
}
}  // namespace dans::stats

#endif  // DANS_STATS_HPP
