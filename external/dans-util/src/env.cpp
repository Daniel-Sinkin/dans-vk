// src/env.cpp
//
#include <dans/env.hpp>
// Externals
#include <dans/development_markers.hpp>
#include <dans/types.hpp>
// StdLib
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
//

namespace dans::env
{
[[nodiscard]] def get(std::string_view name) -> std::optional<std::string>
{
    const std::string name_str{name};
    const auto* raw = std::getenv(name_str.c_str());
    if (raw == nullptr) return std::nullopt;
    return std::string{raw};
}

[[nodiscard]] def get_int(std::string_view name) -> std::optional<i64>
{
    const auto value = get(name);
    if (not value.has_value() or value->empty()) return std::nullopt;
    mut i64 result{};
    const auto* begin = value->data();
    const auto* end = value->data() + value->size();
    const auto [ptr, ec] = std::from_chars(begin, end, result);
    if (ec != std::errc{} or ptr != end) return std::nullopt;
    return result;
}

[[nodiscard]] def get_bool(std::string_view name) -> std::optional<bool>
{
    const auto value = get(name);
    if (not value.has_value() or value->empty()) return std::nullopt;

    mut std::string normalized{};
    normalized.reserve(value->size());
    for (const auto c : *value)
    {
        normalized.push_back(
            static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    constexpr std::array<std::string_view, 4> truthy{"1", "true", "yes", "on"};
    constexpr std::array<std::string_view, 4> falsy{"0", "false", "no", "off"};
    if (std::ranges::find(truthy, normalized) != truthy.end()) return true;
    if (std::ranges::find(falsy, normalized) != falsy.end()) return false;
    return std::nullopt;
}

[[nodiscard]] def get_path(std::string_view name) -> std::optional<std::filesystem::path>
{
    const auto value = get(name);
    if (not value.has_value() or value->empty()) return std::nullopt;
    return std::filesystem::path{*value};
}
}  // namespace dans::env
