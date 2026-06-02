// include/dans/math.hpp
// Externals
#include <dans/development_markers.hpp>
#include <dans/types.hpp>
// StdLib
#include <algorithm>
#include <cassert>
#include <concepts>
#include <limits>
#include <numbers>
#include <utility>
//

#pragma once
#ifndef DANS_MATH_HPP
#define DANS_MATH_HPP

namespace dans::math
{
inline constexpr auto pi = std::numbers::pi;
inline constexpr auto two_pi = 2.0 * pi;
inline constexpr auto tau = two_pi;
inline constexpr auto half_pi = pi / 2.0;
inline constexpr auto quarter_pi = pi / 4.0;
inline constexpr auto inv_pi = std::numbers::inv_pi;
inline constexpr auto e = std::numbers::e;
inline constexpr auto sqrt2 = std::numbers::sqrt2;
inline constexpr auto sqrt3 = std::numbers::sqrt3;
inline constexpr auto inv_sqrt3 = std::numbers::inv_sqrt3;
inline constexpr auto inv_sqrtpi = std::numbers::inv_sqrtpi;
inline constexpr auto ln2 = std::numbers::ln2;
inline constexpr auto ln10 = std::numbers::ln10;

template <typename T, typename U, typename V>
    requires requires(T x, U lo, V hi) {
        std::cmp_less_equal(lo, x);
        std::cmp_less_equal(x, hi);
        std::cmp_less_equal(lo, hi);
    }
[[nodiscard]] constexpr def in_interval(T x, U lo, V hi) -> bool
{
    assert(std::cmp_less_equal(lo, hi));
    return std::cmp_less_equal(lo, x) and std::cmp_less_equal(x, hi);
}

template <std::floating_point T>
[[nodiscard]] constexpr def in_interval(T x, T lo, T hi) -> bool
{
    assert(lo <= hi);
    return lo <= x and x <= hi;
}

template <std::floating_point T>
[[nodiscard]] constexpr def unlerp(T a, T b, T x) -> T
{
    return (x - a) / (b - a);
}

template <std::floating_point T>
[[nodiscard]] constexpr def remap(T x, T in_lo, T in_hi, T out_lo, T out_hi) -> T
{
    return out_lo + (x - in_lo) * (out_hi - out_lo) / (in_hi - in_lo);
}

template <std::floating_point T>
[[nodiscard]] constexpr def approx_equal(
    T a,
    T b,
    T abs_eps = std::numeric_limits<T>::epsilon() * T{100},
    T rel_eps = std::numeric_limits<T>::epsilon() * T{100}) -> bool
{
    if (a == b) return true;
    const auto diff = (a > b) ? (a - b) : (b - a);
    if (diff <= abs_eps) return true;
    const auto abs_a = (a < T{0}) ? -a : a;
    const auto abs_b = (b < T{0}) ? -b : b;
    return diff <= rel_eps * std::max(abs_a, abs_b);
}

template <std::floating_point T>
[[nodiscard]] constexpr def almost_zero(
    T x, T eps = std::numeric_limits<T>::epsilon() * T{100}) -> bool
{
    return ((x < T{0}) ? -x : x) <= eps;
}

template <std::integral T>
[[nodiscard]] constexpr def ceil_div(T a, T b) -> T
{
    assert(a >= T{0} and b > T{0});
    return a / b + (a % b != T{0} ? T{1} : T{0});
}

template <std::integral T>
[[nodiscard]] constexpr def round_up_to(T n, T multiple) -> T
{
    assert(n >= T{0} and multiple > T{0});
    return ceil_div(n, multiple) * multiple;
}

template <std::unsigned_integral T>
[[nodiscard]] constexpr def align_up(T value, T alignment) -> T
{
    assert(alignment > T{0} and (alignment & (alignment - T{1})) == T{0});
    return (value + alignment - T{1}) & ~(alignment - T{1});
}
}  // namespace dans::math

#endif  // DANS_MATH_HPP
