// include/dans/stats.hpp
// Externals
#include <dans/development_markers.hpp>
#include <dans/types.hpp>
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
#define DANS_STATS_HPP

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
    if (values.empty()) return Summary{};

    mut std::vector<f64> sorted{};
    sorted.reserve(values.size());
    for (const auto v : values) sorted.push_back(static_cast<f64>(v));
    std::ranges::sort(sorted);

    mut auto sum = 0.0;
    for (const auto v : sorted) sum += v;
    const auto mean = sum / static_cast<f64>(sorted.size());

    mut auto sq_sum = 0.0;
    for (const auto v : sorted)
    {
        const auto d = v - mean;
        sq_sum += d * d;
    }
    const auto stddev = std::sqrt(sq_sum / static_cast<f64>(sorted.size()));

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
        .mean = mean,
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
