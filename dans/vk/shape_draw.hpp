// dans/vk/shape_draw.hpp
//
#pragma once

#include "dans/vk/types.hpp"
// StdLib
#include <numbers>
//

namespace dans::vk
{

// Shape codes that map to the fragment shader's switch. Public for advanced
// callers that build instances directly; most code should use the DrawList
// convenience methods.
enum class Shape2DType : u32
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

struct CircleConfig
{
    Vec2 center{};
    f32 radius{1.0f};
    Color fill_color{Color::white};
    Color stroke_color{Color::black};
    f32 stroke_width{k_no_stroke};
    bool screen_space{};
};

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

inline constexpr u32 k_shape_flag_dashed = 1u;

}  // namespace dans::vk
