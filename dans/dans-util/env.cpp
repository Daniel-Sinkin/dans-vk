// dans/dans-util/env.cpp
//
#include "dans/dans-util/env.hpp"
// Externals
#include "dans/dans-core/development_markers.hpp"
#include "dans/dans-core/types.hpp"
// StdLib
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <ranges>
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

    const auto* begin = value->data();
    const auto* end = value->data() + value->size();

    mut i64 result{};
    const auto [ptr, ec] = std::from_chars(begin, end, result);
    if (ptr != end or ec != std::errc{}) return std::nullopt;

    return result;
}

[[nodiscard]] def get_bool(std::string_view name) -> std::optional<bool>
{
    using std::views::transform;

    const auto value = get(name);
    if (not value.has_value() or value->empty()) return std::nullopt;

    const auto normalize = [](unsigned char c) -> char
    { return static_cast<char>(std::tolower(c)); };

    const auto normalized = *value | transform(normalize) | std::ranges::to<std::string>();

    constexpr std::array<std::string_view, 4> truthy{"1", "true", "yes", "on"};
    constexpr std::array<std::string_view, 4> falsy{"0", "false", "no", "off"};
    if (std::ranges::contains(truthy, normalized)) return true;
    if (std::ranges::contains(falsy, normalized)) return false;
    return std::nullopt;
}

[[nodiscard]] def get_path(std::string_view name) -> std::optional<std::filesystem::path>
{
    const auto value = get(name);
    if (not value.has_value() or value->empty()) return std::nullopt;
    return std::filesystem::path{*value};
}
}  // namespace dans::env
