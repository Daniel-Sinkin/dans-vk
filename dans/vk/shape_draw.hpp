// dans/vk/shape_draw.hpp
//
#pragma once

#include "dans/vk/config_validity.hpp"
#include "dans/vk/types.hpp"
// StdLib
#include <numbers>
#include <string_view>
//

namespace dans::vk
{

// Shape codes that map to the fragment shader's switch. Public for advanced
// callers that build instances directly; most code should use the DrawList
// convenience methods.
enum class Shape2DType : u8
{
    box = 0u,
    circle = 1u,
    line = 2u,
    sector = 3u,
};

// GPU-facing layout. Matches the vertex input bound by the shape_2d pipeline.
// Most users push these via DrawList::rect / circle / line_2d / sector.
struct Shape2DInstance
{
    Vec4 bounds{};  // x, y, w, h - top-left + size in world units
    Vec4 fill_color{};
    Vec4 stroke_color{};
    Vec4 params0{};
    Vec4 params1{};
    u32 shape_type{};
    u32 flags{};
    f32 pad0_{};
    f32 pad1_{};
};

// Sentinel values so call sites read intent instead of a bare 0.0f.
inline constexpr f32 k_no_stroke = 0.0f;
inline constexpr f32 k_dash_disabled = 0.0f;

struct RectConfig
{
    Vec2 position{};
    Vec2 size{};
    Color fill_color{Color::white};
    Color stroke_color{Color::black};
    f32 stroke_width{k_no_stroke};
    f32 corner_radius{0.0f};
    f32 bevel_size{0.0f};
    bool screen_space{};
};

enum class RectValidity : u8
{
    valid = k_config_is_valid,
    non_positive_size,
};

[[nodiscard]] inline auto validate(const RectConfig& cfg) noexcept -> RectValidity
{
    if (cfg.size.x <= 0.0f or cfg.size.y <= 0.0f) return RectValidity::non_positive_size;
    return RectValidity::valid;
}

// clang-format off
[[nodiscard]] constexpr auto to_string(RectValidity v) noexcept -> std::string_view
{
    switch (v)
    {
        case RectValidity::valid:             return "valid";
        case RectValidity::non_positive_size: return "non_positive_size";
    }
    return "unknown";
}
// clang-format on

struct CircleConfig
{
    Vec2 center{};
    f32 radius{1.0f};
    Color fill_color{Color::white};
    Color stroke_color{Color::black};
    f32 stroke_width{k_no_stroke};
    bool screen_space{};
};

enum class CircleValidity : u8
{
    valid = k_config_is_valid,
    non_positive_radius,
};

[[nodiscard]] inline auto validate(const CircleConfig& cfg) noexcept -> CircleValidity
{
    if (cfg.radius <= 0.0f) return CircleValidity::non_positive_radius;
    return CircleValidity::valid;
}

// clang-format off
[[nodiscard]] constexpr auto to_string(CircleValidity v) noexcept -> std::string_view
{
    switch (v)
    {
        case CircleValidity::valid:               return "valid";
        case CircleValidity::non_positive_radius: return "non_positive_radius";
    }
    return "unknown";
}
// clang-format on

struct Line2DConfig
{
    Vec2 start{};
    Vec2 end{};
    Color color{Color::white};
    f32 thickness{1.0f};
    f32 dash_on{k_dash_disabled};
    f32 dash_off{k_dash_disabled};
    f32 dash_offset{0.0f};
    bool screen_space{};
};

enum class Line2DValidity : u8
{
    valid = k_config_is_valid,
    non_positive_thickness,
    negative_dash,
    inconsistent_dash,
};

[[nodiscard]] inline auto validate(const Line2DConfig& cfg) noexcept -> Line2DValidity
{
    if (cfg.thickness <= 0.0f) return Line2DValidity::non_positive_thickness;
    if (cfg.dash_on < 0.0f or cfg.dash_off < 0.0f) return Line2DValidity::negative_dash;
    if ((cfg.dash_on != k_dash_disabled) != (cfg.dash_off != k_dash_disabled))
    {
        return Line2DValidity::inconsistent_dash;
    }
    return Line2DValidity::valid;
}

// clang-format off
[[nodiscard]] constexpr auto to_string(Line2DValidity v) noexcept -> std::string_view
{
    switch (v)
    {
        case Line2DValidity::valid:                  return "valid";
        case Line2DValidity::non_positive_thickness: return "non_positive_thickness";
        case Line2DValidity::negative_dash:          return "negative_dash";
        case Line2DValidity::inconsistent_dash:      return "inconsistent_dash";
    }
    return "unknown";
}
// clang-format on

struct SectorConfig
{
    Vec2 center{};
    f32 outer_radius{1.0f};
    f32 inner_radius{0.0f};
    f32 start_angle{0.0f};
    f32 end_angle{2.0f * std::numbers::pi_v<f32>};
    Color fill_color{Color::white};
    Color stroke_color{Color::black};
    f32 stroke_width{k_no_stroke};
    bool screen_space{};
};

enum class SectorValidity : u8
{
    valid = k_config_is_valid,
    non_positive_outer_radius,
};

[[nodiscard]] inline auto validate(const SectorConfig& cfg) noexcept -> SectorValidity
{
    if (cfg.outer_radius <= 0.0f) return SectorValidity::non_positive_outer_radius;
    return SectorValidity::valid;
}

// clang-format off
[[nodiscard]] constexpr auto to_string(SectorValidity v) noexcept -> std::string_view
{
    switch (v)
    {
        case SectorValidity::valid:                     return "valid";
        case SectorValidity::non_positive_outer_radius: return "non_positive_outer_radius";
    }
    return "unknown";
}
// clang-format on

struct BezierConfig
{
    Vec2 start{};
    Vec2 control{};
    Vec2 end{};
    Color color{Color::white};
    f32 thickness{1.0f};
    f32 dash_on{k_dash_disabled};
    f32 dash_off{k_dash_disabled};
    usize segments{32zu};
    bool screen_space{};
};

enum class BezierValidity : u8
{
    valid = k_config_is_valid,
    non_positive_thickness,
    zero_segments,
    negative_dash,
    inconsistent_dash,
};

[[nodiscard]] inline auto validate(const BezierConfig& cfg) noexcept -> BezierValidity
{
    if (cfg.thickness <= 0.0f) return BezierValidity::non_positive_thickness;
    if (cfg.segments == 0zu) return BezierValidity::zero_segments;
    if (cfg.dash_on < 0.0f or cfg.dash_off < 0.0f) return BezierValidity::negative_dash;
    if ((cfg.dash_on != k_dash_disabled) != (cfg.dash_off != k_dash_disabled))
    {
        return BezierValidity::inconsistent_dash;
    }
    return BezierValidity::valid;
}

// clang-format off
[[nodiscard]] constexpr auto to_string(BezierValidity v) noexcept -> std::string_view
{
    switch (v)
    {
        case BezierValidity::valid:                  return "valid";
        case BezierValidity::non_positive_thickness: return "non_positive_thickness";
        case BezierValidity::zero_segments:          return "zero_segments";
        case BezierValidity::negative_dash:          return "negative_dash";
        case BezierValidity::inconsistent_dash:      return "inconsistent_dash";
    }
    return "unknown";
}
// clang-format on

inline constexpr u32 k_shape_flag_dashed = 1u;

}  // namespace dans::vk
