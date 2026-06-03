// dans/vk/debug_draw.hpp
//
#pragma once

#include "dans/vk/config_validity.hpp"
#include "dans/vk/types.hpp"
// StdLib
#include <algorithm>
#include <string_view>
//

namespace dans::vk
{
inline constexpr f32 k_debug_line_width{0.012f};
inline constexpr f32 k_debug_arrow_width{0.016f};
inline constexpr f32 k_debug_sphere_width{0.010f};
inline constexpr u32 k_debug_sphere_segments{32u};
inline constexpr f32 k_debug_arrow_min_length_widths{8.0f};

struct DebugLineConfig
{
    Vec3 start{};
    Vec3 end{};
    Color color{Color::white};
    f32 width{k_debug_line_width};
    bool draw_on_top{};
};

enum class DebugLineValidity : u8
{
    valid = k_config_is_valid,
    negative_width,
};

[[nodiscard]] inline auto validate(const DebugLineConfig& cfg) noexcept -> DebugLineValidity
{
    if (cfg.width < 0.0f) return DebugLineValidity::negative_width;
    return DebugLineValidity::valid;
}

// clang-format off
[[nodiscard]] constexpr auto to_string(DebugLineValidity v) noexcept -> std::string_view
{
    switch (v)
    {
        case DebugLineValidity::valid:          return "valid";
        case DebugLineValidity::negative_width: return "negative_width";
    }
    return "unknown";
}
// clang-format on

struct DebugArrowConfig
{
    Vec3 origin{};
    Vec3 vector{};
    Color color{Color::white};
    f32 width{k_debug_arrow_width};
    bool draw_on_top{};
};

enum class DebugArrowValidity : u8
{
    valid = k_config_is_valid,
    negative_width,
    vector_too_short,
};

[[nodiscard]] inline auto validate(const DebugArrowConfig& cfg) noexcept -> DebugArrowValidity
{
    if (cfg.width < 0.0f) return DebugArrowValidity::negative_width;
    const auto min_length = std::max(1.0e-6f, k_debug_arrow_min_length_widths * cfg.width);
    if (glm::dot(cfg.vector, cfg.vector) < min_length * min_length)
    {
        return DebugArrowValidity::vector_too_short;
    }
    return DebugArrowValidity::valid;
}

// clang-format off
[[nodiscard]] constexpr auto to_string(DebugArrowValidity v) noexcept -> std::string_view
{
    switch (v)
    {
        case DebugArrowValidity::valid:            return "valid";
        case DebugArrowValidity::negative_width:   return "negative_width";
        case DebugArrowValidity::vector_too_short: return "vector_too_short";
    }
    return "unknown";
}
// clang-format on

struct DebugSphereConfig
{
    Vec3 center{};
    f32 radius{1.0f};
    Color color{Color::white};
    u32 segments{k_debug_sphere_segments};
    f32 width{k_debug_sphere_width};
    bool draw_on_top{};
};

enum class DebugSphereValidity : u8
{
    valid = k_config_is_valid,
    non_positive_radius,
    too_few_segments,
};

[[nodiscard]] inline auto validate(const DebugSphereConfig& cfg) noexcept -> DebugSphereValidity
{
    if (cfg.radius <= 0.0f) return DebugSphereValidity::non_positive_radius;
    if (cfg.segments < 8u) return DebugSphereValidity::too_few_segments;
    return DebugSphereValidity::valid;
}

// clang-format off
[[nodiscard]] constexpr auto to_string(DebugSphereValidity v) noexcept -> std::string_view
{
    switch (v)
    {
        case DebugSphereValidity::valid:               return "valid";
        case DebugSphereValidity::non_positive_radius: return "non_positive_radius";
        case DebugSphereValidity::too_few_segments:    return "too_few_segments";
    }
    return "unknown";
}
// clang-format on

struct DebugSegment
{
    Vec3 start{};
    f32 width{k_debug_line_width};
    Vec3 end{};
    f32 arrow_tip{};
    Color color{Color::white};
};

}  // namespace dans::vk
