// dans/vk/drawlist.cpp
//
#include "dans/vk/drawlist.hpp"
// StdLib
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <span>
#include <string>
#include <vector>
//

namespace dans::vk
{

auto DrawList::clear() -> void
{
    mesh_commands_.clear();
    debug_segments_.clear();
    debug_on_top_segments_.clear();
    world_text_commands_.clear();
    screen_text_commands_.clear();
    world_shapes_.clear();
    screen_shapes_.clear();
    lights_.clear();
    ambient_light_ = Color{0.035f, 0.040f, 0.050f, 1.0f};
    environment_ = {};
}

auto DrawList::set_ambient_light(Color color) -> void
{
    ambient_light_ = color;
}

auto DrawList::draw_mesh(const MeshDrawConfig& cfg) -> void
{
    if (!cfg.mesh.valid() or cfg.debug.hidden
        or (!cfg.mask.visible_to_camera and !cfg.mask.shadow_producer))
    {
        return;
    }
    mesh_commands_.push_back(
        MeshDrawCommand{
            .mesh = cfg.mesh,
            .object_id = cfg.object_id,
            .transform = cfg.transform,
            .material = cfg.material,
            .mask = cfg.mask,
            .debug = cfg.debug,
        }
    );
}

auto DrawList::draw_basic_mesh(const BasicMeshDrawConfig& cfg) -> void
{
    draw_mesh(
        MeshDrawConfig{
            .mesh = cfg.mesh,
            .object_id = cfg.object_id,
            .transform = cfg.transform,
            .material = Material{.base_color = cfg.color},
            .mask = cfg.mask,
            .debug = cfg.debug,
        }
    );
}

auto DrawList::debug_line(const DebugLineConfig& cfg) -> void
{
    const auto valid = is_valid(cfg);
    assert(valid);
    if (not valid) return;

    auto& segments = cfg.draw_on_top ? debug_on_top_segments_ : debug_segments_;
    segments.push_back(
        DebugSegment{
            .start = cfg.start,
            .width = cfg.width,
            .end = cfg.end,
            .arrow_tip = 0.0f,
            .color = cfg.color,
        }
    );
}

auto DrawList::debug_arrow(const DebugArrowConfig& cfg) -> void
{
    const auto valid = is_valid(cfg);
    assert(valid);
    if (not valid) return;

    auto& segments = cfg.draw_on_top ? debug_on_top_segments_ : debug_segments_;
    segments.push_back(
        DebugSegment{
            .start = cfg.origin,
            .width = cfg.width,
            .end = cfg.origin + cfg.vector,
            .arrow_tip = 1.0f,
            .color = cfg.color,
        }
    );
}

auto DrawList::debug_sphere(const DebugSphereConfig& cfg) -> void
{
    const auto valid = is_valid(cfg);
    assert(valid);
    if (not valid) return;

    const auto segments_f = static_cast<f32>(cfg.segments);
    for (auto i = 0u; i < cfg.segments; ++i)
    {
        const auto pi2 = 2.0f * std::numbers::pi_v<f32>;
        const auto t0 = pi2 * static_cast<f32>(i) / segments_f;
        const auto t1 = pi2 * static_cast<f32>(i + 1u) / segments_f;
        const auto c0 = std::cos(t0) * cfg.radius;
        const auto s0 = std::sin(t0) * cfg.radius;
        const auto c1 = std::cos(t1) * cfg.radius;
        const auto s1 = std::sin(t1) * cfg.radius;
        debug_line(
            DebugLineConfig{
                .start = cfg.center + Vec3{c0, s0, 0.0f},
                .end = cfg.center + Vec3{c1, s1, 0.0f},
                .color = cfg.color,
                .width = cfg.width,
                .draw_on_top = cfg.draw_on_top,
            }
        );
        debug_line(
            DebugLineConfig{
                .start = cfg.center + Vec3{c0, 0.0f, s0},
                .end = cfg.center + Vec3{c1, 0.0f, s1},
                .color = cfg.color,
                .width = cfg.width,
                .draw_on_top = cfg.draw_on_top,
            }
        );
        debug_line(
            DebugLineConfig{
                .start = cfg.center + Vec3{0.0f, c0, s0},
                .end = cfg.center + Vec3{0.0f, c1, s1},
                .color = cfg.color,
                .width = cfg.width,
                .draw_on_top = cfg.draw_on_top,
            }
        );
    }
}

auto DrawList::add_light(const LightConfig& cfg) -> void
{
    if (!cfg.enabled)
    {
        return;
    }
    lights_.push_back(cfg);
}

auto DrawList::directional_light(const DirectionalLightConfig& cfg) -> void
{
    add_light(
        LightConfig{
            .type = LightType::directional,
            .direction = cfg.direction,
            .color = cfg.color,
            .intensity = cfg.intensity,
            .shadow = cfg.shadow,
            .enabled = cfg.enabled,
        }
    );
}

auto DrawList::radial_light(const RadialLightConfig& cfg) -> void
{
    add_light(
        LightConfig{
            .type = LightType::radial,
            .position = cfg.position,
            .color = cfg.color,
            .intensity = cfg.intensity,
            .range = cfg.range,
            .enabled = cfg.enabled,
        }
    );
}

auto DrawList::spot_light(const SpotLightConfig& cfg) -> void
{
    add_light(
        LightConfig{
            .type = LightType::spot,
            .position = cfg.position,
            .direction = cfg.direction,
            .color = cfg.color,
            .intensity = cfg.intensity,
            .range = cfg.range,
            .inner_cone_angle = cfg.inner_cone_angle,
            .outer_cone_angle = cfg.outer_cone_angle,
            .shadow = cfg.shadow,
            .enabled = cfg.enabled,
        }
    );
}

auto DrawList::set_environment(const EnvironmentConfig& cfg) -> void
{
    environment_ = cfg;
}

auto DrawList::text(const TextDrawConfig& cfg) -> void
{
    if (cfg.text.empty() or cfg.size_scale <= 0.0f)
    {
        return;
    }
    world_text_commands_.push_back(
        TextDrawCommand{
            .position = cfg.position,
            .text = std::string{cfg.text},
            .color = cfg.color,
            .size_scale = cfg.size_scale,
        }
    );
}

auto DrawList::text_screen(const TextScreenConfig& cfg) -> void
{
    if (cfg.text.empty() or cfg.size_scale <= 0.0f)
    {
        return;
    }
    screen_text_commands_.push_back(
        TextDrawCommand{
            .position = cfg.position,
            .text = std::string{cfg.text},
            .color = cfg.color,
            .size_scale = cfg.size_scale,
        }
    );
}

auto DrawList::rect(const RectConfig& cfg) -> void
{
    const auto valid = is_valid(cfg);
    assert(valid);
    if (not valid) return;

    auto& list = cfg.screen_space ? screen_shapes_ : world_shapes_;
    list.push_back(
        Shape2DInstance{
            .bounds = Vec4{cfg.position.x, cfg.position.y, cfg.size.x, cfg.size.y},
            .fill_color = to_vec4(cfg.fill_color),
            .stroke_color = to_vec4(cfg.stroke_color),
            .params0 = Vec4{cfg.corner_radius, cfg.bevel_size, cfg.stroke_width, 0.0f},
            .params1 = Vec4{0.0f},
            .shape_type = static_cast<u32>(Shape2DType::box),
            .flags = 0u,
        }
    );
}

auto DrawList::circle(const CircleConfig& cfg) -> void
{
    const auto valid = is_valid(cfg);
    assert(valid);
    if (not valid) return;

    const auto diameter = 2.0f * cfg.radius;
    auto& list = cfg.screen_space ? screen_shapes_ : world_shapes_;
    list.push_back(
        Shape2DInstance{
            .bounds =
                Vec4{cfg.center.x - cfg.radius, cfg.center.y - cfg.radius, diameter, diameter},
            .fill_color = to_vec4(cfg.fill_color),
            .stroke_color = to_vec4(cfg.stroke_color),
            .params0 = Vec4{0.0f, 0.0f, cfg.stroke_width, 0.0f},
            .params1 = Vec4{0.0f},
            .shape_type = static_cast<u32>(Shape2DType::circle),
            .flags = 0u,
        }
    );
}

auto DrawList::line_2d(const Line2DConfig& cfg) -> void
{
    const auto valid = is_valid(cfg);
    assert(valid);
    if (not valid) return;

    const auto pad = cfg.thickness * 0.5f + 1.0f;
    const auto min_x = std::min(cfg.start.x, cfg.end.x) - pad;
    const auto min_y = std::min(cfg.start.y, cfg.end.y) - pad;
    const auto max_x = std::max(cfg.start.x, cfg.end.x) + pad;
    const auto max_y = std::max(cfg.start.y, cfg.end.y) + pad;
    const auto dashed = (cfg.dash_on != k_dash_disabled);
    auto& list = cfg.screen_space ? screen_shapes_ : world_shapes_;
    list.push_back(
        Shape2DInstance{
            .bounds = Vec4{min_x, min_y, max_x - min_x, max_y - min_y},
            .fill_color = to_vec4(cfg.color),
            .stroke_color = Vec4{0.0f},
            .params0 =
                Vec4{
                    cfg.start.x - min_x, cfg.start.y - min_y, cfg.end.x - min_x, cfg.end.y - min_y
                },
            .params1 = Vec4{cfg.thickness, cfg.dash_on, cfg.dash_off, cfg.dash_offset},
            .shape_type = static_cast<u32>(Shape2DType::line),
            .flags = dashed ? k_shape_flag_dashed : 0u,
        }
    );
}

auto DrawList::sector(const SectorConfig& cfg) -> void
{
    const auto valid = is_valid(cfg);
    assert(valid);
    if (not valid) return;

    const auto diameter = 2.0f * cfg.outer_radius;
    auto& list = cfg.screen_space ? screen_shapes_ : world_shapes_;
    list.push_back(
        Shape2DInstance{
            .bounds =
                Vec4{
                    cfg.center.x - cfg.outer_radius,
                    cfg.center.y - cfg.outer_radius,
                    diameter,
                    diameter,
                },
            .fill_color = to_vec4(cfg.fill_color),
            .stroke_color = to_vec4(cfg.stroke_color),
            .params0 = Vec4{cfg.inner_radius, cfg.outer_radius, cfg.start_angle, cfg.end_angle},
            .params1 = Vec4{cfg.stroke_width, 0.0f, 0.0f, 0.0f},
            .shape_type = static_cast<u32>(Shape2DType::sector),
            .flags = 0u,
        }
    );
}

auto DrawList::bezier(const BezierConfig& cfg) -> void
{
    const auto valid = is_valid(cfg);
    assert(valid);
    if (not valid) return;

    const auto step = 1.0f / static_cast<f32>(cfg.segments);
    auto previous = cfg.start;
    auto cumulative_arc = 0.0f;
    for (auto i = 0zu; i < cfg.segments; ++i)
    {
        const auto t = static_cast<f32>(i + 1zu) * step;
        const auto one_minus_t = 1.0f - t;
        const auto w_start = one_minus_t * one_minus_t;
        const auto w_control = 2.0f * one_minus_t * t;
        const auto w_end = t * t;
        const auto next = w_start * cfg.start + w_control * cfg.control + w_end * cfg.end;
        line_2d(
            Line2DConfig{
                .start = previous,
                .end = next,
                .color = cfg.color,
                .thickness = cfg.thickness,
                .dash_on = cfg.dash_on,
                .dash_off = cfg.dash_off,
                .dash_offset = cumulative_arc,
                .screen_space = cfg.screen_space,
            }
        );
        cumulative_arc += glm::distance(previous, next);
        previous = next;
    }
}

auto DrawList::mesh_commands() const noexcept -> std::span<const MeshDrawCommand>
{
    return std::span<const MeshDrawCommand>{mesh_commands_.data(), mesh_commands_.size()};
}

auto DrawList::debug_segments() const noexcept -> std::span<const DebugSegment>
{
    return std::span<const DebugSegment>{debug_segments_.data(), debug_segments_.size()};
}

auto DrawList::debug_on_top_segments() const noexcept -> std::span<const DebugSegment>
{
    return std::span<const DebugSegment>{
        debug_on_top_segments_.data(),
        debug_on_top_segments_.size(),
    };
}

auto DrawList::world_text_commands() const noexcept -> std::span<const TextDrawCommand>
{
    return std::span<const TextDrawCommand>{
        world_text_commands_.data(),
        world_text_commands_.size(),
    };
}

auto DrawList::screen_text_commands() const noexcept -> std::span<const TextDrawCommand>
{
    return std::span<const TextDrawCommand>{
        screen_text_commands_.data(),
        screen_text_commands_.size(),
    };
}

auto DrawList::world_shapes() const noexcept -> std::span<const Shape2DInstance>
{
    return std::span<const Shape2DInstance>{world_shapes_.data(), world_shapes_.size()};
}

auto DrawList::screen_shapes() const noexcept -> std::span<const Shape2DInstance>
{
    return std::span<const Shape2DInstance>{screen_shapes_.data(), screen_shapes_.size()};
}

auto DrawList::lights() const noexcept -> std::span<const LightConfig>
{
    return std::span<const LightConfig>{lights_.data(), lights_.size()};
}

auto DrawList::ambient_light() const noexcept -> Color
{
    return ambient_light_;
}

auto DrawList::environment() const noexcept -> const EnvironmentConfig&
{
    return environment_;
}

}  // namespace dans::vk
