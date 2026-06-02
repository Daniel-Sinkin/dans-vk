// include/dans/str.hpp
// Externals
#include <dans/development_markers.hpp>
// StdLib
#include <concepts>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
//

#pragma once
#ifndef DANS_STR_HPP
#define DANS_STR_HPP

namespace dans::str
{
inline constexpr std::string_view default_whitespace{" \t\n\r\f\v"};

[[nodiscard]] def split(std::string_view text, char delim) -> std::vector<std::string_view>;

[[nodiscard]] constexpr def ltrim(
    std::string_view text, std::string_view chars = default_whitespace) -> std::string_view
{
    const auto pos = text.find_first_not_of(chars);
    if (pos == std::string_view::npos) return std::string_view{};
    return text.substr(pos);
}

[[nodiscard]] constexpr def rtrim(
    std::string_view text, std::string_view chars = default_whitespace) -> std::string_view
{
    const auto pos = text.find_last_not_of(chars);
    if (pos == std::string_view::npos) return std::string_view{};
    return text.substr(0, pos + 1);
}

[[nodiscard]] constexpr def trim(
    std::string_view text, std::string_view chars = default_whitespace) -> std::string_view
{
    return rtrim(ltrim(text, chars), chars);
}

template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, std::string_view>
[[nodiscard]] def join(R&& range, std::string_view sep) -> std::string
{
    mut std::string out{};
    mut auto first = true;
    for (const std::string_view part : range)
    {
        if (not first) out += sep;
        out += part;
        first = false;
    }
    return out;
}

}  // namespace dans::str

#endif  // DANS_STR_HPP
