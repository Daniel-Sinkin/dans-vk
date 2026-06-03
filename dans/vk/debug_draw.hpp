#pragma once

#include "dans/vk/types.hpp"

namespace dans::vk
{

// Default visual sizing for debug primitives, centralized as dans-vk framework
// styling instead of per-struct magic numbers.
inline constexpr f32 k_debug_line_width = 0.012f;
inline constexpr f32 k_debug_arrow_width = 0.016f;
inline constexpr f32 k_debug_sphere_width = 0.010f;
inline constexpr u32 k_debug_sphere_segments = 32u;

struct DebugLineConfig
{
    Vec3 start{};
    Vec3 end{};
    Color color{Color::white};
    f32 width{k_debug_line_width};
    bool draw_on_top{};
};

struct DebugArrowConfig
{
    Vec3 origin{};
    Vec3 vector{};
    Color color{Color::white};
    f32 width{k_debug_arrow_width};
    bool draw_on_top{};
};

struct DebugSphereConfig
{
    Vec3 center{};
    f32 radius{1.0f};
    Color color{Color::white};
    u32 segments{k_debug_sphere_segments};
    f32 width{k_debug_sphere_width};
    bool draw_on_top{};
};

struct DebugSegment
{
    Vec3 start{};
    f32 width{k_debug_line_width};
    Vec3 end{};
    f32 arrow_tip{};
    Color color{Color::white};
};

}  // namespace dans::vk
