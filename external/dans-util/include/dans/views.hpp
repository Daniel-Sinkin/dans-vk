// include/dans/views.hpp
// Externals
#include <dans/development_markers.hpp>
#include <dans/types.hpp>
// StdLib
#include <ranges>
#include <utility>
#include <version>
//

#pragma once
#ifndef DANS_VIEWS_HPP
#define DANS_VIEWS_HPP

namespace dans::views
{
using std::views::zip;

#if defined(__cpp_lib_ranges_enumerate) and __cpp_lib_ranges_enumerate >= 202302L
using std::views::enumerate;
#else
template <std::ranges::viewable_range R>
[[nodiscard]] constexpr def enumerate(R&& r) -> decltype(auto)
{
    return std::views::zip(std::views::iota(usize{0}), std::forward<R>(r));
}
#endif
}  // namespace dans::views

#endif  // DANS_VIEWS_HPP
