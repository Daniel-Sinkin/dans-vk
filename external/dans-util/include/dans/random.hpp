// include/dans/random.hpp
// Externals
#include <dans/development_markers.hpp>
#include <dans/types.hpp>
// StdLib
#include <concepts>
#include <random>
//

#pragma once
#ifndef DANS_RANDOM_HPP
#define DANS_RANDOM_HPP

namespace dans::random
{
[[nodiscard]] def rng() -> std::mt19937_64&;
def seed(u64 value) -> void;

template <std::integral T>
[[nodiscard]] def int_in(T lo, T hi) -> T
{
    std::uniform_int_distribution<T> dist{lo, hi};
    return dist(rng());
}

template <std::floating_point T = f64>
[[nodiscard]] def float_in(T lo, T hi) -> T
{
    std::uniform_real_distribution<T> dist{lo, hi};
    return dist(rng());
}

template <std::floating_point T = f64>
[[nodiscard]] def normal(T mean = T{0}, T stddev = T{1}) -> T
{
    std::normal_distribution<T> dist{mean, stddev};
    return dist(rng());
}
}  // namespace dans::random

#endif  // DANS_RANDOM_HPP
