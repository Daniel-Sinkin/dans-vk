// app/main_2d.cpp
//
// Self-contained tensor network visualiser app on top of dans-vk.
// Excluded from this draft: contraction, Julia/HTTP interop, JSONL loading.
//
#include "dans/vk/math.hpp"
#include "dans/vk/runtime.hpp"
#include "dans/vk/shape_draw.hpp"
#include "dans/vk/text_draw.hpp"
#include "dans/vk/types.hpp"
// StdLib
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <map>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
//

namespace
{

using dans::vk::Color;
using dans::vk::FrameContext;
using dans::vk::Runtime;
using dans::vk::Vec2;
using dans::vk::f32;
using dans::vk::f64;
using dans::vk::i32;
using dans::vk::u32;
using dans::vk::u8;
using dans::vk::usize;

// ============================================================================
// Constants
// ============================================================================

constexpr f32 k_node_min_radius = 22.0f;
constexpr f32 k_node_radius_step = 3.0f;
constexpr f32 k_node_bevel_ratio = 0.32f;

constexpr f32 k_leg_length = 32.0f;
constexpr f32 k_leg_thickness = 2.5f;
constexpr f32 k_leg_tip_radius = 3.8f;
constexpr f32 k_leg_hit_radius = 14.0f;

constexpr f32 k_bond_thickness = 3.0f;
constexpr f32 k_bond_lane_spacing = 18.0f;

constexpr f32 k_grid_spacing = 50.0f;

constexpr i32 k_radial_outer_count = 8;
constexpr f32 k_radial_inner_radius = 38.0f;
constexpr f32 k_radial_outer_radius = 100.0f;

constexpr f32 k_drag_threshold_px = 4.0f;

// ============================================================================
// Color helpers
// ============================================================================

[[nodiscard]] auto tag_color(std::string_view tag) noexcept -> Color
{
    if (tag == "phys")
    {
        return Color{0.95f, 0.43f, 0.40f, 1.0f};
    }
    if (tag == "hlink")
    {
        return Color{0.45f, 0.78f, 0.50f, 1.0f};
    }
    if (tag == "vlink")
    {
        return Color{0.46f, 0.74f, 1.0f, 1.0f};
    }
    return Color{0.67f, 0.72f, 0.80f, 1.0f};
}

[[nodiscard]] auto rank_color(i32 rank) noexcept -> Color
{
    switch (rank)
    {
        case 0:
            return Color{0.36f, 0.40f, 0.50f, 1.0f};
        case 1:
            return Color{0.36f, 0.62f, 0.92f, 1.0f};
        case 2:
            return Color{0.95f, 0.55f, 0.30f, 1.0f};
        case 3:
            return Color{0.46f, 0.80f, 0.55f, 1.0f};
        case 4:
            return Color{0.88f, 0.55f, 0.88f, 1.0f};
        case 5:
            return Color{0.95f, 0.85f, 0.45f, 1.0f};
        case 6:
            return Color{0.85f, 0.65f, 0.40f, 1.0f};
        default:
            return Color{0.65f, 0.70f, 0.78f, 1.0f};
    }
}

[[nodiscard]] auto node_radius(i32 rank) noexcept -> f32
{
    return k_node_min_radius + k_node_radius_step * static_cast<f32>(std::clamp(rank, 0, 5));
}

using dans::vk::with_alpha;

// ============================================================================
// Scene types
// ============================================================================

struct LegInfo
{
    std::vector<std::string> tags{};
    i32 prime_level{};
    f32 angle{};
};

struct Tensor
{
    u32 id{};
    Vec2 position{};
    i32 rank{};
    Color color{};
    std::string label{};
    std::vector<LegInfo> legs{};
};

struct Bond
{
    u32 id{};
    u32 tensor_a{};
    i32 leg_a{};
    u32 tensor_b{};
    i32 leg_b{};
    i32 prime_level{};
    std::vector<std::string> tags{};
};

struct Abstraction
{
    std::string type_name{};
    std::vector<u32> tensor_ids{};
};

class Scene
{
  public:
    std::vector<Tensor> tensors{};
    std::vector<Bond> bonds{};
    std::vector<Abstraction> abstractions{};

    auto allocate_id() noexcept -> u32
    {
        return next_id_++;
    }

    [[nodiscard]] auto tensor_by_id(u32 id) -> Tensor*
    {
        for (auto& t : tensors)
        {
            if (t.id == id)
            {
                return &t;
            }
        }
        return nullptr;
    }

    [[nodiscard]] auto tensor_by_id(u32 id) const -> const Tensor*
    {
        for (const auto& t : tensors)
        {
            if (t.id == id)
            {
                return &t;
            }
        }
        return nullptr;
    }

    [[nodiscard]] auto bond_by_id(u32 id) -> Bond*
    {
        for (auto& b : bonds)
        {
            if (b.id == id)
            {
                return &b;
            }
        }
        return nullptr;
    }

    auto add_tensor(Vec2 position, i32 rank, std::string label = {}) -> u32
    {
        const auto id = allocate_id();
        if (label.empty())
        {
            label = std::format("T{}", id);
        }
        tensors.push_back(
            Tensor{
                .id = id,
                .position = position,
                .rank = rank,
                .color = rank_color(rank),
                .label = std::move(label),
                .legs = std::vector<LegInfo>(static_cast<usize>(std::max(0, rank))),
            }
        );
        return id;
    }

    auto add_bond(u32 ta, i32 la, u32 tb, i32 lb, std::string tag, i32 prime = 0) -> u32
    {
        const auto id = allocate_id();
        std::vector<std::string> tags;
        if (not tag.empty())
        {
            tags.push_back(std::move(tag));
        }
        bonds.push_back(
            Bond{
                .id = id,
                .tensor_a = ta,
                .leg_a = la,
                .tensor_b = tb,
                .leg_b = lb,
                .prime_level = prime,
                .tags = std::move(tags),
            }
        );
        return id;
    }

    [[nodiscard]] auto is_leg_bonded(u32 tensor_id, i32 leg_index) const -> bool
    {
        for (const auto& b : bonds)
        {
            if ((b.tensor_a == tensor_id and b.leg_a == leg_index)
                or (b.tensor_b == tensor_id and b.leg_b == leg_index))
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] auto bonded_partner(u32 tensor_id, i32 leg_index) const -> std::optional<u32>
    {
        for (const auto& b : bonds)
        {
            if (b.tensor_a == tensor_id and b.leg_a == leg_index)
            {
                return b.tensor_b;
            }
            if (b.tensor_b == tensor_id and b.leg_b == leg_index)
            {
                return b.tensor_a;
            }
        }
        return std::nullopt;
    }

    auto delete_tensor(u32 id) -> void
    {
        std::erase_if(bonds, [id](const Bond& b) {
            return b.tensor_a == id or b.tensor_b == id;
        });
        for (auto& a : abstractions)
        {
            std::erase(a.tensor_ids, id);
        }
        std::erase_if(tensors, [id](const Tensor& t) { return t.id == id; });
    }

    auto delete_bond(u32 id) -> void
    {
        std::erase_if(bonds, [id](const Bond& b) { return b.id == id; });
    }

  private:
    u32 next_id_{1};
};

// ============================================================================
// Geometry helpers
// ============================================================================

[[nodiscard]] auto length2(Vec2 v) noexcept -> f32
{
    return v.x * v.x + v.y * v.y;
}

[[nodiscard]] auto length(Vec2 v) noexcept -> f32
{
    return std::sqrt(length2(v));
}

[[nodiscard]] auto normalize_or(Vec2 v, Vec2 fallback) noexcept -> Vec2
{
    const auto len = length(v);
    if (len < 1e-6f)
    {
        return fallback;
    }
    return Vec2{v.x / len, v.y / len};
}

[[nodiscard]] auto perp(Vec2 v) noexcept -> Vec2
{
    return Vec2{-v.y, v.x};
}

// Bonded leg direction = unit vector from tensor center toward partner.
[[nodiscard]] auto leg_direction(const Scene& scene, const Tensor& t, i32 leg_index) noexcept -> Vec2
{
    const auto partner_id = scene.bonded_partner(t.id, leg_index);
    if (partner_id.has_value())
    {
        const auto* partner = scene.tensor_by_id(*partner_id);
        if (partner != nullptr)
        {
            return normalize_or(partner->position - t.position, Vec2{1.0f, 0.0f});
        }
    }
    if (leg_index < 0 or leg_index >= static_cast<i32>(t.legs.size()))
    {
        return Vec2{1.0f, 0.0f};
    }
    const auto a = t.legs[static_cast<usize>(leg_index)].angle;
    return Vec2{std::cos(a), std::sin(a)};
}

[[nodiscard]] auto leg_anchor(const Scene& scene, const Tensor& t, i32 leg_index) noexcept -> Vec2
{
    const auto dir = leg_direction(scene, t, leg_index);
    const auto r = node_radius(t.rank);
    return Vec2{t.position.x + dir.x * r, t.position.y + dir.y * r};
}

[[nodiscard]] auto leg_tip(const Scene& scene, const Tensor& t, i32 leg_index) noexcept -> Vec2
{
    const auto dir = leg_direction(scene, t, leg_index);
    const auto r = node_radius(t.rank);
    const auto extent = r + k_leg_length;
    return Vec2{t.position.x + dir.x * extent, t.position.y + dir.y * extent};
}

// Multi-lane offset (in world units) for a bond inside a group of N parallel bonds.
[[nodiscard]] auto lane_offset(i32 lane_index, i32 total) noexcept -> f32
{
    if (total <= 1)
    {
        return 0.0f;
    }
    const auto center = (static_cast<f32>(total) - 1.0f) * 0.5f;
    return (static_cast<f32>(lane_index) - center) * k_bond_lane_spacing;
}

[[nodiscard]] auto quad_bezier(Vec2 a, Vec2 b, Vec2 c, f32 t) noexcept -> Vec2
{
    const auto omt = 1.0f - t;
    return Vec2{
        omt * omt * a.x + 2.0f * omt * t * b.x + t * t * c.x,
        omt * omt * a.y + 2.0f * omt * t * b.y + t * t * c.y,
    };
}

// ============================================================================
// Visualizer
// ============================================================================

enum class MouseAction : u8
{
    idle,
    pressed,
    dragging_tensors,
    rotating_leg,
    box_selecting,
};

struct LegTarget
{
    u32 tensor{};
    i32 leg_index{};
};

struct RadialItem
{
    std::string label{};
};

class Visualizer
{
  public:
    auto setup(Runtime& runtime) -> void
    {
        runtime_ = &runtime;
        seed_scene();

        runtime.set_camera_2d(scene_center(), 1.1f);

#if defined(DANS_VK_DEFAULT_FONT_PATH)
        runtime.load_font(
            {
                .ttf_path = DANS_VK_DEFAULT_FONT_PATH,
                .pixel_size = 26.0f,
            }
        );
#endif
    }

    auto update(FrameContext& frame, const f32 dt) -> void
    {
        elapsed_ += dt;
        const auto& input = frame.input;
        const auto mouse_world = runtime_->screen_to_world_2d(input.mouse_px);

        update_hover(input, mouse_world);
        handle_keyboard(input, mouse_world);
        handle_mouse(input, mouse_world);

        draw_grid(frame);
        draw_abstractions(frame);
        draw_bonds(frame);
        draw_free_legs(frame);
        draw_tensors(frame);
        draw_box_select(frame);
        draw_radial_menu(frame);
        draw_hud(frame, mouse_world);
    }

  private:
    Runtime* runtime_{};
    Scene scene_{};

    std::vector<u32> selected_tensors_{};
    std::optional<u32> hovered_tensor_{};
    std::optional<LegTarget> hovered_leg_{};

    MouseAction mouse_action_{MouseAction::idle};
    Vec2 mouse_press_world_{};
    Vec2 mouse_press_px_{};
    std::vector<Vec2> drag_initial_positions_{};
    std::optional<LegTarget> rotating_leg_{};
    f32 rotating_initial_angle_{};
    Vec2 box_min_world_{};
    Vec2 box_max_world_{};

    bool radial_open_{};
    Vec2 radial_center_px_{};
    i32 radial_hovered_{-1};
    std::string radial_last_action_{};

    f32 elapsed_{};

    // ------------------------------------------------------------------
    // Scene setup
    // ------------------------------------------------------------------

    auto seed_scene() -> void
    {
        const f32 spacing = 130.0f;
        const f32 mps_y = 0.0f;
        const f32 mpo_y = 180.0f;
        constexpr i32 n_mps = 8;
        constexpr i32 n_mpo = 4;

        std::vector<u32> mps_ids;
        for (i32 i = 0; i < n_mps; ++i)
        {
            const auto rank = (i == 0 or i == n_mps - 1) ? 2 : 3;
            const auto pos = Vec2{static_cast<f32>(i) * spacing, mps_y};
            const auto id = scene_.add_tensor(pos, rank, std::format("T{}", i + 1));
            mps_ids.push_back(id);
            auto* tensor = scene_.tensor_by_id(id);
            tensor->legs[0].tags = {"phys"};
            tensor->legs[0].angle = std::numbers::pi_v<f32> * 0.5f;
        }
        for (i32 i = 0; i + 1 < n_mps; ++i)
        {
            const auto left_leg = (i == 0) ? 1 : 2;
            scene_.add_bond(mps_ids[static_cast<usize>(i)], left_leg, mps_ids[static_cast<usize>(i + 1)], 1, "hlink");
        }

        std::vector<u32> mpo_ids;
        for (i32 i = 0; i < n_mpo; ++i)
        {
            const auto pos = Vec2{
                (static_cast<f32>(i) + 0.5f) * spacing * 2.0f - spacing,
                mpo_y,
            };
            const auto id = scene_.add_tensor(pos, 4, std::format("M{}", i + 1));
            mpo_ids.push_back(id);
            auto* tensor = scene_.tensor_by_id(id);
            tensor->legs[0].tags = {"phys"};
            tensor->legs[0].angle = std::numbers::pi_v<f32> * 0.5f;
            tensor->legs[1].tags = {"phys"};
            tensor->legs[1].angle = -std::numbers::pi_v<f32> * 0.5f;
        }

        scene_.add_bond(mpo_ids[0], 2, mps_ids[0], 0, "vlink");
        scene_.add_bond(mpo_ids[0], 3, mps_ids[1], 0, "vlink");
        scene_.add_bond(mpo_ids[1], 2, mps_ids[2], 0, "vlink");
        scene_.add_bond(mpo_ids[1], 3, mps_ids[3], 0, "vlink");
        scene_.add_bond(mpo_ids[2], 2, mps_ids[4], 0, "vlink");
        scene_.add_bond(mpo_ids[2], 3, mps_ids[5], 0, "vlink");
        scene_.add_bond(mpo_ids[3], 2, mps_ids[6], 0, "vlink");
        scene_.add_bond(mpo_ids[3], 3, mps_ids[7], 0, "vlink");

        // Add a couple of demonstrative parallel bonds: extra hlinks across one pair
        scene_.add_bond(mps_ids[2], 2, mps_ids[3], 1, "hlink");
        scene_.add_bond(mps_ids[2], 2, mps_ids[3], 1, "vlink", 1);

        scene_.abstractions.push_back({.type_name = "MPS", .tensor_ids = mps_ids});
        scene_.abstractions.push_back({.type_name = "MPO", .tensor_ids = mpo_ids});
    }

    [[nodiscard]] auto scene_center() const noexcept -> Vec2
    {
        if (scene_.tensors.empty())
        {
            return Vec2{0.0f, 0.0f};
        }
        f32 sx = 0.0f;
        f32 sy = 0.0f;
        for (const auto& t : scene_.tensors)
        {
            sx += t.position.x;
            sy += t.position.y;
        }
        const auto n = static_cast<f32>(scene_.tensors.size());
        return Vec2{sx / n, sy / n};
    }

    // ------------------------------------------------------------------
    // Selection helpers
    // ------------------------------------------------------------------

    [[nodiscard]] auto is_selected(u32 id) const noexcept -> bool
    {
        return std::find(selected_tensors_.begin(), selected_tensors_.end(), id)
               != selected_tensors_.end();
    }

    auto toggle_selected(u32 id) -> void
    {
        const auto it = std::find(selected_tensors_.begin(), selected_tensors_.end(), id);
        if (it == selected_tensors_.end())
        {
            selected_tensors_.push_back(id);
        }
        else
        {
            selected_tensors_.erase(it);
        }
    }

    // ------------------------------------------------------------------
    // Hit testing
    // ------------------------------------------------------------------

    [[nodiscard]] auto hit_tensor(Vec2 world_pos) const -> std::optional<u32>
    {
        for (auto it = scene_.tensors.rbegin(); it != scene_.tensors.rend(); ++it)
        {
            const auto r = node_radius(it->rank);
            if (std::abs(world_pos.x - it->position.x) <= r
                and std::abs(world_pos.y - it->position.y) <= r)
            {
                return it->id;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] auto hit_leg_tip(Vec2 world_pos) const -> std::optional<LegTarget>
    {
        const auto zoom = runtime_->camera_2d_zoom();
        const auto threshold = k_leg_hit_radius * zoom;
        for (const auto& t : scene_.tensors)
        {
            for (i32 leg_i = 0; leg_i < t.rank; ++leg_i)
            {
                if (scene_.is_leg_bonded(t.id, leg_i))
                {
                    continue;
                }
                const auto tip = leg_tip(scene_, t, leg_i);
                const auto d = world_pos - tip;
                if (length2(d) <= threshold * threshold)
                {
                    return LegTarget{.tensor = t.id, .leg_index = leg_i};
                }
            }
        }
        return std::nullopt;
    }

    auto update_hover(const dans::vk::InputState& input, Vec2 mouse_world) -> void
    {
        if (input.mouse_captured_by_ui)
        {
            hovered_tensor_ = std::nullopt;
            hovered_leg_ = std::nullopt;
            return;
        }
        hovered_leg_ = hit_leg_tip(mouse_world);
        hovered_tensor_ = hovered_leg_.has_value() ? std::nullopt : hit_tensor(mouse_world);

        if (radial_open_)
        {
            radial_hovered_ = radial_sector_under(input.mouse_px);
        }
        else
        {
            radial_hovered_ = -1;
        }
    }

    // ------------------------------------------------------------------
    // Mouse handling
    // ------------------------------------------------------------------

    auto handle_mouse(const dans::vk::InputState& input, Vec2 mouse_world) -> void
    {
        if (radial_open_)
        {
            if (input.left_click.occurred)
            {
                close_radial(true);
            }
            return;
        }

        if (input.left_click.occurred and not input.mouse_captured_by_ui)
        {
            mouse_press_world_ = mouse_world;
            mouse_press_px_ = input.mouse_px;
            mouse_action_ = MouseAction::pressed;
            if (hovered_leg_.has_value())
            {
                rotating_leg_ = hovered_leg_;
                const auto* t = scene_.tensor_by_id(rotating_leg_->tensor);
                if (t != nullptr)
                {
                    rotating_initial_angle_
                        = t->legs[static_cast<usize>(rotating_leg_->leg_index)].angle;
                }
                if (not input.left_click.modifiers.shift)
                {
                    selected_tensors_.clear();
                    selected_tensors_.push_back(rotating_leg_->tensor);
                }
            }
            else if (hovered_tensor_.has_value())
            {
                const auto id = *hovered_tensor_;
                if (input.left_click.modifiers.shift)
                {
                    toggle_selected(id);
                }
                else if (not is_selected(id))
                {
                    selected_tensors_.clear();
                    selected_tensors_.push_back(id);
                }
                drag_initial_positions_.clear();
                drag_initial_positions_.reserve(selected_tensors_.size());
                for (const auto sid : selected_tensors_)
                {
                    if (const auto* t = scene_.tensor_by_id(sid); t != nullptr)
                    {
                        drag_initial_positions_.push_back(t->position);
                    }
                    else
                    {
                        drag_initial_positions_.push_back(Vec2{0.0f, 0.0f});
                    }
                }
            }
            else
            {
                if (not input.left_click.modifiers.shift)
                {
                    selected_tensors_.clear();
                }
                box_min_world_ = mouse_world;
                box_max_world_ = mouse_world;
            }
        }

        if (mouse_action_ == MouseAction::pressed and input.left_button_down)
        {
            const auto px_delta = input.mouse_px - mouse_press_px_;
            if (std::abs(px_delta.x) > k_drag_threshold_px
                or std::abs(px_delta.y) > k_drag_threshold_px)
            {
                if (rotating_leg_.has_value())
                {
                    mouse_action_ = MouseAction::rotating_leg;
                }
                else if (hovered_tensor_.has_value()
                         or (not selected_tensors_.empty()
                             and (not drag_initial_positions_.empty())))
                {
                    mouse_action_ = MouseAction::dragging_tensors;
                }
                else
                {
                    mouse_action_ = MouseAction::box_selecting;
                }
            }
        }

        if (mouse_action_ == MouseAction::rotating_leg and rotating_leg_.has_value())
        {
            auto* t = scene_.tensor_by_id(rotating_leg_->tensor);
            if (t != nullptr)
            {
                const auto v = mouse_world - t->position;
                if (length2(v) > 1.0f)
                {
                    t->legs[static_cast<usize>(rotating_leg_->leg_index)].angle
                        = std::atan2(v.y, v.x);
                }
            }
        }
        else if (mouse_action_ == MouseAction::dragging_tensors)
        {
            const auto world_delta = mouse_world - mouse_press_world_;
            for (auto i = 0u; i < selected_tensors_.size() and i < drag_initial_positions_.size();
                 ++i)
            {
                auto* t = scene_.tensor_by_id(selected_tensors_[i]);
                if (t != nullptr)
                {
                    t->position = drag_initial_positions_[i] + world_delta;
                }
            }
        }
        else if (mouse_action_ == MouseAction::box_selecting)
        {
            box_min_world_ = Vec2{
                std::min(mouse_press_world_.x, mouse_world.x),
                std::min(mouse_press_world_.y, mouse_world.y),
            };
            box_max_world_ = Vec2{
                std::max(mouse_press_world_.x, mouse_world.x),
                std::max(mouse_press_world_.y, mouse_world.y),
            };
        }

        if (mouse_action_ != MouseAction::idle and not input.left_button_down)
        {
            if (mouse_action_ == MouseAction::box_selecting)
            {
                finalize_box_select(input.shift_held);
            }
            mouse_action_ = MouseAction::idle;
            rotating_leg_ = std::nullopt;
            drag_initial_positions_.clear();
        }
    }

    auto finalize_box_select(bool additive) -> void
    {
        if (not additive)
        {
            selected_tensors_.clear();
        }
        for (const auto& t : scene_.tensors)
        {
            if (t.position.x >= box_min_world_.x and t.position.x <= box_max_world_.x
                and t.position.y >= box_min_world_.y and t.position.y <= box_max_world_.y)
            {
                if (not is_selected(t.id))
                {
                    selected_tensors_.push_back(t.id);
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // Keyboard handling
    // ------------------------------------------------------------------

    auto handle_keyboard(const dans::vk::InputState& input, Vec2 mouse_world) -> void
    {
        if (input.space_pressed)
        {
            if (radial_open_)
            {
                close_radial(true);
            }
            else
            {
                open_radial(input.mouse_px);
            }
        }
        if (input.key_n_pressed and not radial_open_)
        {
            const auto id = scene_.add_tensor(mouse_world, 3);
            selected_tensors_ = {id};
        }
        if (input.key_delete_pressed)
        {
            const auto copy = selected_tensors_;
            for (const auto id : copy)
            {
                scene_.delete_tensor(id);
            }
            selected_tensors_.clear();
        }
        if (input.key_plus_pressed and hovered_leg_.has_value())
        {
            if (auto* t = scene_.tensor_by_id(hovered_leg_->tensor); t != nullptr)
            {
                t->legs[static_cast<usize>(hovered_leg_->leg_index)].prime_level += 1;
            }
        }
        if (input.key_minus_pressed and hovered_leg_.has_value())
        {
            if (auto* t = scene_.tensor_by_id(hovered_leg_->tensor); t != nullptr)
            {
                auto& level = t->legs[static_cast<usize>(hovered_leg_->leg_index)].prime_level;
                level = std::max(0, level - 1);
            }
        }
    }

    // ------------------------------------------------------------------
    // Radial menu
    // ------------------------------------------------------------------

    [[nodiscard]] auto radial_items() const noexcept -> std::array<RadialItem, k_radial_outer_count>
    {
        return {
            RadialItem{"contract"},
            RadialItem{"split"},
            RadialItem{"prime"},
            RadialItem{"duplicate"},
            RadialItem{"delete"},
            RadialItem{"connect"},
            RadialItem{"cut"},
            RadialItem{"label"},
        };
    }

    auto open_radial(Vec2 cursor_px) -> void
    {
        radial_open_ = true;
        radial_center_px_ = cursor_px;
        radial_hovered_ = -1;
    }

    auto close_radial(bool commit) -> void
    {
        if (commit and radial_hovered_ >= 0)
        {
            const auto items = radial_items();
            radial_last_action_ = items[static_cast<usize>(radial_hovered_)].label;
        }
        radial_open_ = false;
        radial_hovered_ = -1;
    }

    [[nodiscard]] auto radial_sector_under(Vec2 cursor_px) const -> i32
    {
        const auto v = cursor_px - radial_center_px_;
        const auto d2 = length2(v);
        const auto in2 = k_radial_inner_radius * k_radial_inner_radius;
        const auto out2 = k_radial_outer_radius * k_radial_outer_radius;
        if (d2 < in2 or d2 > out2)
        {
            return -1;
        }
        constexpr auto two_pi = 2.0f * std::numbers::pi_v<f32>;
        auto angle = std::atan2(v.y, v.x);
        if (angle < 0.0f)
        {
            angle += two_pi;
        }
        const auto sector = static_cast<i32>(
            angle * static_cast<f32>(k_radial_outer_count) / two_pi
        );
        return std::clamp(sector, 0, k_radial_outer_count - 1);
    }

    // ------------------------------------------------------------------
    // Bond layout (multi-lane)
    // ------------------------------------------------------------------

    struct LaneKey
    {
        u32 lo{};
        u32 hi{};
        bool operator<(const LaneKey& other) const noexcept
        {
            return std::tie(lo, hi) < std::tie(other.lo, other.hi);
        }
    };

    [[nodiscard]] auto compute_bond_lanes() const -> std::map<u32, std::pair<i32, i32>>
    {
        // Map bond id -> (lane_index, total_lanes)
        std::map<LaneKey, std::vector<u32>> groups;
        for (const auto& b : scene_.bonds)
        {
            const auto lo = std::min(b.tensor_a, b.tensor_b);
            const auto hi = std::max(b.tensor_a, b.tensor_b);
            groups[LaneKey{lo, hi}].push_back(b.id);
        }
        std::map<u32, std::pair<i32, i32>> result;
        for (const auto& [key, ids] : groups)
        {
            for (auto i = 0u; i < ids.size(); ++i)
            {
                result[ids[i]] = {static_cast<i32>(i), static_cast<i32>(ids.size())};
            }
        }
        return result;
    }

    // ------------------------------------------------------------------
    // Drawing
    // ------------------------------------------------------------------

    auto draw_grid(FrameContext& frame) -> void
    {
        const auto pivot = runtime_->camera_2d_pivot();
        const auto zoom = runtime_->camera_2d_zoom();
        const auto logical = runtime_->logical_extent();
        const auto w = logical.x;
        const auto h = logical.y;
        const auto half_w = 0.5f * w * zoom;
        const auto half_h = 0.5f * h * zoom;

        // Adapt spacing based on zoom so we don't draw a million dots when zoomed out
        auto spacing = k_grid_spacing;
        while (spacing / zoom < 18.0f)
        {
            spacing *= 2.0f;
        }
        const auto dot_radius = std::max(0.6f, 1.4f * zoom);
        const auto x_start = static_cast<i32>(std::floor((pivot.x - half_w) / spacing)) - 1;
        const auto x_end = static_cast<i32>(std::ceil((pivot.x + half_w) / spacing)) + 1;
        const auto y_start = static_cast<i32>(std::floor((pivot.y - half_h) / spacing)) - 1;
        const auto y_end = static_cast<i32>(std::ceil((pivot.y + half_h) / spacing)) + 1;

        const auto color = Color{1.0f, 1.0f, 1.0f, 0.10f};
        for (auto xi = x_start; xi <= x_end; ++xi)
        {
            for (auto yi = y_start; yi <= y_end; ++yi)
            {
                frame.draw.circle(
                    {
                        .center = Vec2{static_cast<f32>(xi) * spacing, static_cast<f32>(yi) * spacing},
                        .radius = dot_radius,
                        .fill_color = color,
                    }
                );
            }
        }
    }

    auto draw_abstractions(FrameContext& frame) -> void
    {
        for (const auto& a : scene_.abstractions)
        {
            if (a.tensor_ids.empty())
            {
                continue;
            }
            std::optional<Vec2> mn;
            std::optional<Vec2> mx;
            f32 max_radius = 0.0f;
            for (const auto id : a.tensor_ids)
            {
                const auto* t = scene_.tensor_by_id(id);
                if (t == nullptr)
                {
                    continue;
                }
                const auto r = node_radius(t->rank);
                max_radius = std::max(max_radius, r);
                const auto p = t->position;
                if (not mn.has_value())
                {
                    mn = p;
                    mx = p;
                }
                else
                {
                    mn = Vec2{std::min(mn->x, p.x), std::min(mn->y, p.y)};
                    mx = Vec2{std::max(mx->x, p.x), std::max(mx->y, p.y)};
                }
            }
            if (not mn.has_value())
            {
                continue;
            }
            const auto pad = max_radius + 38.0f;
            const auto top_left = Vec2{mn->x - pad, mn->y - pad};
            const auto size = Vec2{(mx->x - mn->x) + 2.0f * pad, (mx->y - mn->y) + 2.0f * pad};
            frame.draw.rect(
                {
                    .position = top_left,
                    .size = size,
                    .fill_color = Color{0.42f, 0.58f, 0.82f, 0.05f},
                    .stroke_color = Color{0.58f, 0.74f, 0.96f, 0.55f},
                    .stroke_width = 1.6f,
                    .corner_radius = 28.0f,
                }
            );
            frame.draw.text(
                {
                    .position = Vec2{top_left.x + 16.0f, top_left.y + 24.0f},
                    .text = std::format("{} ({})", a.type_name, a.tensor_ids.size()),
                    .color = Color{0.84f, 0.92f, 1.0f, 0.92f},
                    .size_scale = 0.55f,
                }
            );
        }
    }

    auto draw_bonds(FrameContext& frame) -> void
    {
        const auto lanes = compute_bond_lanes();
        for (const auto& bond : scene_.bonds)
        {
            const auto* a = scene_.tensor_by_id(bond.tensor_a);
            const auto* b = scene_.tensor_by_id(bond.tensor_b);
            if (a == nullptr or b == nullptr)
            {
                continue;
            }
            const auto from = a->position;
            const auto to = b->position;
            const auto along = to - from;
            const auto mid = Vec2{(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f};
            const auto along_norm = normalize_or(along, Vec2{1.0f, 0.0f});
            const auto perp_norm = perp(along_norm);

            const auto it = lanes.find(bond.id);
            const auto [lane_index, total]
                = (it != lanes.end()) ? it->second : std::pair<i32, i32>{0, 1};
            const auto offset = lane_offset(lane_index, total);
            // Curve bulge: even for lane 0 of a single bond, no bulge. Otherwise the
            // bulge is the lane offset itself, producing parallel arcs.
            const auto bulge_amount = (total <= 1) ? 0.0f : offset;
            const auto control = Vec2{
                mid.x + perp_norm.x * bulge_amount,
                mid.y + perp_norm.y * bulge_amount,
            };

            const auto tag = bond.tags.empty() ? std::string{} : bond.tags.front();
            const auto base_color = tag_color(tag);
            const auto dashed = (bond.prime_level > 0);
            frame.draw.bezier(
                {
                    .start = from,
                    .control = control,
                    .end = to,
                    .color = base_color,
                    .thickness = k_bond_thickness,
                    .dash_on = dashed ? 9.0f : 0.0f,
                    .dash_off = dashed ? 6.0f : 0.0f,
                    .segments = 28u,
                }
            );

            // Bond label at quarter-curve point so multi-lane labels don't overlap badly
            if (not bond.tags.empty() or bond.prime_level > 0)
            {
                const auto t_label = (lane_index % 2 == 0) ? 0.35f : 0.65f;
                const auto label_pos = quad_bezier(from, control, to, t_label);
                draw_bond_label(frame, label_pos, bond, base_color);
            }
        }
    }

    auto draw_bond_label(
        FrameContext& frame, Vec2 world_pos, const Bond& bond, Color base_color
    ) -> void
    {
        std::vector<std::string> lines;
        for (const auto& t : bond.tags)
        {
            lines.push_back(t);
        }
        if (bond.prime_level > 0)
        {
            std::string ticks;
            if (bond.prime_level <= 3)
            {
                ticks.assign(static_cast<usize>(bond.prime_level), '\'');
            }
            else
            {
                ticks = std::format("{}'", bond.prime_level);
            }
            lines.push_back(ticks);
        }
        if (lines.empty())
        {
            return;
        }
        // Approximate label width: longest line * font width
        const auto approx_glyph_width = 8.5f;
        f32 longest = 0.0f;
        for (const auto& line : lines)
        {
            longest = std::max(longest, static_cast<f32>(line.size()) * approx_glyph_width);
        }
        const auto pad_x = 6.0f;
        const auto pad_y = 4.0f;
        const auto line_height = 14.0f;
        const auto width = longest + 2.0f * pad_x;
        const auto height = line_height * static_cast<f32>(lines.size()) + 2.0f * pad_y;
        const auto top_left = Vec2{world_pos.x - width * 0.5f, world_pos.y - height * 0.5f};

        frame.draw.rect(
            {
                .position = top_left,
                .size = Vec2{width, height},
                .fill_color = Color{0.06f, 0.10f, 0.14f, 0.86f},
                .stroke_color = with_alpha(base_color, 0.6f),
                .stroke_width = 1.0f,
                .corner_radius = 6.0f,
            }
        );

        for (auto i = 0u; i < lines.size(); ++i)
        {
            frame.draw.text(
                {
                    .position = Vec2{
                        top_left.x + pad_x,
                        top_left.y + pad_y + line_height * (static_cast<f32>(i) + 0.7f),
                    },
                    .text = lines[i],
                    .color = with_alpha(base_color, 0.95f),
                    .size_scale = 0.45f,
                }
            );
        }
    }

    auto draw_free_legs(FrameContext& frame) -> void
    {
        for (const auto& t : scene_.tensors)
        {
            for (i32 leg_i = 0; leg_i < t.rank; ++leg_i)
            {
                if (scene_.is_leg_bonded(t.id, leg_i))
                {
                    continue;
                }
                const auto& info = t.legs[static_cast<usize>(leg_i)];
                const auto tag = info.tags.empty() ? std::string{} : info.tags.front();
                const auto color = tag_color(tag);
                const auto anchor = leg_anchor(scene_, t, leg_i);
                const auto tip = leg_tip(scene_, t, leg_i);
                const auto dashed = (info.prime_level > 0);

                frame.draw.line_2d(
                    {
                        .start = anchor,
                        .end = tip,
                        .color = color,
                        .thickness = k_leg_thickness,
                        .dash_on = dashed ? 6.0f : 0.0f,
                        .dash_off = dashed ? 4.0f : 0.0f,
                    }
                );

                const auto highlight = hovered_leg_.has_value()
                                       and hovered_leg_->tensor == t.id
                                       and hovered_leg_->leg_index == leg_i;
                if (highlight)
                {
                    frame.draw.circle(
                        {
                            .center = tip,
                            .radius = k_leg_tip_radius * 2.4f,
                            .fill_color = with_alpha(color, 0.30f),
                        }
                    );
                }
                frame.draw.circle(
                    {
                        .center = tip,
                        .radius = k_leg_tip_radius,
                        .fill_color = color,
                        .stroke_color = Color{0.08f, 0.10f, 0.14f, 0.80f},
                        .stroke_width = 0.8f,
                    }
                );

                draw_leg_label(frame, t, leg_i, tip, color, info);
            }
        }
    }

    auto draw_leg_label(
        FrameContext& frame,
        const Tensor& t,
        i32 leg_i,
        Vec2 tip,
        Color color,
        const LegInfo& info
    ) -> void
    {
        std::vector<std::string> lines;
        for (const auto& tag : info.tags)
        {
            lines.push_back(tag);
        }
        if (info.prime_level > 0)
        {
            std::string ticks;
            if (info.prime_level <= 3)
            {
                ticks.assign(static_cast<usize>(info.prime_level), '\'');
            }
            else
            {
                ticks = std::format("{}'", info.prime_level);
            }
            lines.push_back(ticks);
        }
        if (lines.empty())
        {
            return;
        }
        const auto dir = leg_direction(scene_, t, leg_i);
        const auto offset = Vec2{dir.x * 14.0f, dir.y * 14.0f};
        const auto line_height = 13.0f;
        for (auto i = 0u; i < lines.size(); ++i)
        {
            const auto y_offset = static_cast<f32>(i) * line_height;
            // Position labels outward from the tip, along the leg direction
            const auto pos = Vec2{
                tip.x + offset.x - 12.0f,
                tip.y + offset.y + 4.0f + y_offset,
            };
            frame.draw.text(
                {
                    .position = pos,
                    .text = lines[i],
                    .color = with_alpha(color, 0.9f),
                    .size_scale = 0.42f,
                }
            );
        }
    }

    auto draw_tensors(FrameContext& frame) -> void
    {
        for (const auto& t : scene_.tensors)
        {
            const auto r = node_radius(t.rank);
            const auto selected = is_selected(t.id);
            const auto hovered = (hovered_tensor_.has_value() and *hovered_tensor_ == t.id);

            if (selected)
            {
                frame.draw.rect(
                    {
                        .position = Vec2{t.position.x - r - 4.0f, t.position.y - r - 4.0f},
                        .size = Vec2{(r + 4.0f) * 2.0f, (r + 4.0f) * 2.0f},
                        .fill_color = Color{1.0f, 1.0f, 1.0f, 0.10f},
                        .stroke_color = Color{0.0f, 0.0f, 0.0f, 0.0f},
                        .stroke_width = 0.0f,
                        .corner_radius = r * k_node_bevel_ratio + 4.0f,
                    }
                );
            }

            auto stroke = Color{0.05f, 0.07f, 0.10f, 0.85f};
            auto stroke_w = 1.6f;
            if (selected)
            {
                stroke = Color{1.0f, 1.0f, 1.0f, 1.0f};
                stroke_w = 3.0f;
            }
            else if (hovered)
            {
                stroke = Color{1.0f, 1.0f, 1.0f, 0.70f};
                stroke_w = 2.2f;
            }

            frame.draw.rect(
                {
                    .position = Vec2{t.position.x - r, t.position.y - r},
                    .size = Vec2{2.0f * r, 2.0f * r},
                    .fill_color = t.color,
                    .stroke_color = stroke,
                    .stroke_width = stroke_w,
                    .bevel_size = r * k_node_bevel_ratio,
                }
            );

            const auto label_width_approx = static_cast<f32>(t.label.size()) * 8.0f;
            frame.draw.text(
                {
                    .position = Vec2{t.position.x - label_width_approx * 0.5f, t.position.y + 5.0f},
                    .text = t.label,
                    .color = Color{1.0f, 1.0f, 1.0f, 1.0f},
                    .size_scale = 0.55f,
                }
            );
        }
    }

    auto draw_box_select(FrameContext& frame) -> void
    {
        if (mouse_action_ != MouseAction::box_selecting)
        {
            return;
        }
        const auto size = box_max_world_ - box_min_world_;
        frame.draw.rect(
            {
                .position = box_min_world_,
                .size = size,
                .fill_color = Color{0.40f, 0.65f, 1.0f, 0.10f},
                .stroke_color = Color{0.55f, 0.75f, 1.0f, 0.80f},
                .stroke_width = 1.5f,
                .corner_radius = 4.0f,
            }
        );
    }

    auto draw_radial_menu(FrameContext& frame) -> void
    {
        if (not radial_open_)
        {
            return;
        }
        const auto items = radial_items();
        constexpr auto two_pi = 2.0f * std::numbers::pi_v<f32>;
        for (i32 i = 0; i < k_radial_outer_count; ++i)
        {
            const auto a0 = two_pi * static_cast<f32>(i) / static_cast<f32>(k_radial_outer_count);
            const auto a1 = two_pi * static_cast<f32>(i + 1) / static_cast<f32>(k_radial_outer_count);
            const auto a_mid = 0.5f * (a0 + a1);
            const auto is_hover = (radial_hovered_ == i);
            const Color base{0.20f, 0.28f, 0.40f, 0.90f};
            const Color hover{0.42f, 0.62f, 0.88f, 0.95f};
            frame.draw.sector(
                {
                    .center = radial_center_px_,
                    .outer_radius = k_radial_outer_radius,
                    .inner_radius = k_radial_inner_radius,
                    .start_angle = a0,
                    .end_angle = a1,
                    .fill_color = is_hover ? hover : base,
                    .stroke_color = Color{0.06f, 0.10f, 0.16f, 0.95f},
                    .stroke_width = 1.0f,
                    .screen_space = true,
                }
            );
            const auto label_radius = 0.5f * (k_radial_inner_radius + k_radial_outer_radius);
            const auto label_pos = Vec2{
                radial_center_px_.x + std::cos(a_mid) * label_radius - 22.0f,
                radial_center_px_.y + std::sin(a_mid) * label_radius + 4.0f,
            };
            frame.draw.text_screen(
                {
                    .position = label_pos,
                    .text = items[static_cast<usize>(i)].label,
                    .color = Color::white,
                    .size_scale = 0.55f,
                }
            );
        }
        frame.draw.circle(
            {
                .center = radial_center_px_,
                .radius = k_radial_inner_radius - 4.0f,
                .fill_color = Color{0.10f, 0.13f, 0.18f, 0.96f},
                .stroke_color = Color{0.60f, 0.72f, 0.92f, 0.85f},
                .stroke_width = 1.5f,
                .screen_space = true,
            }
        );
        frame.draw.text_screen(
            {
                .position = Vec2{radial_center_px_.x - 16.0f, radial_center_px_.y + 5.0f},
                .text = "menu",
                .color = Color{0.86f, 0.92f, 1.0f, 0.95f},
                .size_scale = 0.50f,
            }
        );
    }

    auto draw_hud(FrameContext& frame, Vec2 mouse_world) -> void
    {
        frame.draw.text_screen(
            {
                .position = Vec2{24.0f, 36.0f},
                .text = "dans_vk tensor network visualiser",
                .color = Color::white,
            }
        );

        const auto info = std::format(
            "tensors: {}  bonds: {}  selected: {}  zoom: {:.2f}  world: ({:.0f}, {:.0f})",
            scene_.tensors.size(),
            scene_.bonds.size(),
            selected_tensors_.size(),
            static_cast<f64>(runtime_->camera_2d_zoom()),
            static_cast<f64>(mouse_world.x),
            static_cast<f64>(mouse_world.y)
        );
        frame.draw.text_screen(
            {
                .position = Vec2{24.0f, 70.0f},
                .text = info,
                .color = Color{0.80f, 0.88f, 0.98f, 1.0f},
                .size_scale = 0.62f,
            }
        );
        frame.draw.text_screen(
            {
                .position = Vec2{24.0f, 96.0f},
                .text = "LMB select/drag  shift+LMB multi  drag leg tip rotates  RMB/MMB pan  wheel zoom",
                .color = Color{0.62f, 0.74f, 0.90f, 1.0f},
                .size_scale = 0.50f,
            }
        );
        frame.draw.text_screen(
            {
                .position = Vec2{24.0f, 118.0f},
                .text = "N add tensor  Del/Backspace delete selected  + / - prime/unprime hovered leg  Space radial menu",
                .color = Color{0.62f, 0.74f, 0.90f, 1.0f},
                .size_scale = 0.50f,
            }
        );
        if (not radial_last_action_.empty())
        {
            frame.draw.text_screen(
                {
                    .position = Vec2{24.0f, 144.0f},
                    .text = std::format("last menu action: {}", radial_last_action_),
                    .color = Color{0.98f, 0.86f, 0.66f, 1.0f},
                    .size_scale = 0.55f,
                }
            );
        }
    }
};

[[nodiscard]] auto parse_u32(const char* text, const u32 fallback) noexcept -> u32
{
    char* end = nullptr;
    const auto value = std::strtoul(text, &end, 10);
    if (end == text)
    {
        return fallback;
    }
    return static_cast<u32>(value);
}

auto print_usage(const char* executable) -> void
{
    std::cout << "usage: " << executable
              << " [--smoke-frames N] [--screenshot PATH] [--hide-ui]\n";
}

}  // namespace

auto main(int argc, char** argv) -> int
{
    try
    {
        dans::vk::RuntimeConfig config{
            .window_title = "dans_vk tensor network visualiser",
            .render_mode = dans::vk::RenderMode::two_d,
        };
        for (auto i = 1; i < argc; ++i)
        {
            const std::string_view arg{argv[i]};
            if (arg == "--help")
            {
                print_usage(argv[0]);
                return 0;
            }
            if (arg == "--smoke-frames" and i + 1 < argc)
            {
                config.smoke_frames = parse_u32(argv[++i], 0u);
            }
            else if (arg == "--screenshot" and i + 1 < argc)
            {
                config.screenshot_path = argv[++i];
            }
            else if (arg == "--hide-ui")
            {
                config.hide_ui = true;
            }
            else
            {
                std::cerr << "unknown or incomplete argument: " << arg << '\n';
                print_usage(argv[0]);
                return 2;
            }
        }

        Visualizer app{};
        dans::vk::Runtime runtime{std::move(config)};
        return runtime.run_prototype(app);
    }
    catch (const std::exception& error)
    {
        std::cerr << "fatal: " << error.what() << '\n';
        return 1;
    }
}
