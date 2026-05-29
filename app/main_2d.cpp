// app/main_2d.cpp
//
// Native tensor network visualiser app. Renders the same data model
// the HTML/JS visualiser uses, loads .json / .jsonl envelope files,
// and POSTs to the Julia /contract server.
//
// What lives in this file: the SDL/Vulkan glue + the interactive
// visualiser. Data structures, envelope serde, HTTP client, template
// builders and JSONL loader live under app/viz/ as headers so the whole
// app moves cleanly to its own repo with main_2d.cpp + app/viz/ in tow.
//
#include "app/viz/envelope.hpp"
#include "app/viz/jsonl.hpp"
#include "app/viz/julia_client.hpp"
#include "app/viz/templates.hpp"
#include "app/viz/types.hpp"
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
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
//

namespace
{

using viz::Abstraction;
using viz::Bond;
using viz::Color;
using viz::f32;
using viz::f64;
using viz::Frame;
using viz::i32;
using viz::JuliaClient;
using viz::JuliaClientConfig;
using viz::Tensor;
using viz::u32;
using viz::u8;
using viz::usize;
using viz::Vec2;
using dans::vk::FrameContext;
using dans::vk::Runtime;
using dans::vk::with_alpha;

// ============================================================================
// Rendering constants
// ============================================================================

constexpr f32 k_node_min_radius = 24.0f;
constexpr f32 k_node_radius_step = 3.5f;
constexpr f32 k_node_max_radius = 60.0f;
constexpr f32 k_node_radius_r0 = 18.0f;
constexpr f32 k_node_bevel_ratio = 0.32f;

constexpr f32 k_leg_length = 30.0f;
constexpr f32 k_leg_thickness = 2.5f;
constexpr f32 k_leg_tip_radius = 3.6f;
constexpr f32 k_leg_hit_radius = 14.0f;

constexpr f32 k_bond_thickness = 3.0f;
constexpr f32 k_bond_lane_offset = 16.0f;

constexpr f32 k_grid_spacing = 50.0f;

constexpr i32 k_radial_outer_count = 8;
constexpr f32 k_radial_inner_radius = 36.0f;
constexpr f32 k_radial_outer_radius = 100.0f;

constexpr f32 k_drag_threshold_px = 4.0f;

[[nodiscard]] auto node_radius(i32 rank) noexcept -> f32
{
    if (rank == 0)
    {
        return k_node_radius_r0;
    }
    return std::min(k_node_max_radius, k_node_min_radius + static_cast<f32>(rank) * k_node_radius_step);
}

// ============================================================================
// Lookup helpers
// ============================================================================

[[nodiscard]] auto find_tensor(Frame& f, u32 id) -> Tensor*
{
    for (auto& t : f.tensors)
    {
        if (t.id == id)
        {
            return &t;
        }
    }
    return nullptr;
}

[[nodiscard]] auto find_tensor(const Frame& f, u32 id) -> const Tensor*
{
    for (const auto& t : f.tensors)
    {
        if (t.id == id)
        {
            return &t;
        }
    }
    return nullptr;
}

[[nodiscard]] auto is_leg_bonded(const Frame& f, u32 tensor_id, i32 leg_index) -> bool
{
    for (const auto& b : f.bonds)
    {
        if ((b.a == tensor_id and b.ai == leg_index) or (b.b == tensor_id and b.bi == leg_index))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto bonded_partner(const Frame& f, u32 tensor_id, i32 leg_index)
    -> std::optional<u32>
{
    for (const auto& b : f.bonds)
    {
        if (b.a == tensor_id and b.ai == leg_index)
        {
            return b.b;
        }
        if (b.b == tensor_id and b.bi == leg_index)
        {
            return b.a;
        }
    }
    return std::nullopt;
}

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

[[nodiscard]] auto leg_direction(const Frame& f, const Tensor& t, i32 leg_index) noexcept -> Vec2
{
    const auto partner = bonded_partner(f, t.id, leg_index);
    if (partner.has_value())
    {
        const auto* p = find_tensor(f, *partner);
        if (p != nullptr)
        {
            return normalize_or(Vec2{p->x - t.x, p->y - t.y}, Vec2{1.0f, 0.0f});
        }
    }
    if (leg_index < 0 or static_cast<usize>(leg_index) >= t.leg_angles.size())
    {
        return Vec2{1.0f, 0.0f};
    }
    const auto a = t.leg_angles[static_cast<usize>(leg_index)];
    return Vec2{std::cos(a), std::sin(a)};
}

// Ray-cast from the tensor center along angle `a` to the rounded-rect boundary.
// Mirrors legAnchor() in tensor-network-visualiser.html: corner radius is
// r * k_node_bevel_ratio (clamped to >= 4 px), flat edges between corners.
// Returns the contact point and the outward boundary normal.
struct LegAnchor
{
    Vec2 point{};
    Vec2 normal{};
};

[[nodiscard]] auto leg_anchor_geom(Vec2 center, f32 r, f32 a) noexcept -> LegAnchor
{
    const auto corner = std::max(4.0f, r * k_node_bevel_ratio);
    const auto flat = r - corner;
    const auto ca = std::cos(a);
    const auto sa = std::sin(a);
    const auto ax = std::fabs(ca);
    const auto ay = std::fabs(sa);
    if (ax < 1e-9f and ay < 1e-9f)
    {
        return LegAnchor{.point = Vec2{center.x + r, center.y}, .normal = Vec2{1.0f, 0.0f}};
    }
    const auto s = std::max(ax, ay);
    const auto k = r / s;
    const auto ex = center.x + ca * k;
    const auto ey = center.y + sa * k;
    if (ax >= ay)
    {
        if (std::fabs(ey - center.y) <= flat)
        {
            return LegAnchor{
                .point = Vec2{ex, ey},
                .normal = Vec2{ca > 0.0f ? 1.0f : -1.0f, 0.0f},
            };
        }
    }
    else
    {
        if (std::fabs(ex - center.x) <= flat)
        {
            return LegAnchor{
                .point = Vec2{ex, ey},
                .normal = Vec2{0.0f, sa > 0.0f ? 1.0f : -1.0f},
            };
        }
    }
    const auto sx = ca > 0.0f ? 1.0f : -1.0f;
    const auto sy = sa > 0.0f ? 1.0f : -1.0f;
    const auto ccx = center.x + sx * flat;
    const auto ccy = center.y + sy * flat;
    const auto dx = ccx - center.x;
    const auto dy = ccy - center.y;
    const auto B = -2.0f * (dx * ca + dy * sa);
    const auto C = dx * dx + dy * dy - corner * corner;
    const auto disc = B * B - 4.0f * C;
    if (disc < 0.0f)
    {
        return LegAnchor{.point = Vec2{ex, ey}, .normal = Vec2{ca, sa}};
    }
    const auto ts = (-B + std::sqrt(disc)) * 0.5f;
    const auto px = center.x + ts * ca;
    const auto py = center.y + ts * sa;
    auto nx = px - ccx;
    auto ny = py - ccy;
    const auto nl = std::sqrt(nx * nx + ny * ny);
    if (nl > 1e-6f)
    {
        nx /= nl;
        ny /= nl;
    }
    else
    {
        nx = ca;
        ny = sa;
    }
    return LegAnchor{.point = Vec2{px, py}, .normal = Vec2{nx, ny}};
}

[[nodiscard]] auto leg_anchor(const Frame& f, const Tensor& t, i32 leg_index) noexcept -> LegAnchor
{
    const auto dir = leg_direction(f, t, leg_index);
    const auto a = std::atan2(dir.y, dir.x);
    return leg_anchor_geom(Vec2{t.x, t.y}, node_radius(t.rank), a);
}

[[nodiscard]] auto leg_tip(const Frame& f, const Tensor& t, i32 leg_index) noexcept -> Vec2
{
    const auto anc = leg_anchor(f, t, leg_index);
    return Vec2{anc.point.x + anc.normal.x * k_leg_length, anc.point.y + anc.normal.y * k_leg_length};
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

struct AppArgs
{
    std::optional<std::filesystem::path> load_path{};
    JuliaClientConfig julia{};
    bool no_julia{};
    bool seed_demo{true};
};

class Visualizer
{
  public:
    explicit Visualizer(AppArgs args) : args_(std::move(args))
    {
    }

    auto setup(Runtime& runtime) -> void
    {
        runtime_ = &runtime;
        if (not args_.no_julia)
        {
            julia_.emplace(args_.julia);
            julia_available_ = julia_->healthz();
            if (julia_available_)
            {
                set_status("Julia server reachable at " + args_.julia.host + ":"
                           + std::to_string(args_.julia.port));
            }
            else
            {
                set_status(
                    "Julia server NOT reachable at " + args_.julia.host + ":"
                    + std::to_string(args_.julia.port)
                );
            }
        }
        if (args_.load_path.has_value())
        {
            load_path(*args_.load_path);
        }
        if (frames_.empty() and args_.seed_demo)
        {
            seed_demo_scene();
        }
        if (frames_.empty())
        {
            frames_.push_back(Frame{});
        }
        apply_frame(0);
        runtime.set_camera_2d(scene_center(), 1.1f);

#if defined(DANS_VK_DEFAULT_FONT_PATH)
        runtime.load_font(
            {
                .ttf_path = DANS_VK_DEFAULT_FONT_PATH,
                .pixel_size = 22.0f,
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
    AppArgs args_{};
    std::optional<JuliaClient> julia_{};
    bool julia_available_{};

    std::vector<Frame> frames_{};
    usize current_frame_{};
    std::string frame_source_{};

    Frame& live() noexcept
    {
        return frames_[current_frame_];
    }
    [[nodiscard]] const Frame& live() const noexcept
    {
        return frames_[current_frame_];
    }

    // Selection model follows the HTML: standalone tensors live in
    // selected_tensors_, containers in selected_abstractions_, and a single
    // drilled-into tensor (visual highlight inside a selected container) in
    // inner_sel_.
    std::vector<u32> selected_tensors_{};
    std::vector<u32> selected_abstractions_{};
    std::optional<u32> inner_sel_{};
    std::optional<u32> hovered_tensor_{};
    std::optional<u32> hovered_abstraction_{};
    std::optional<LegTarget> hovered_leg_{};

    MouseAction mouse_action_{MouseAction::idle};
    Vec2 mouse_press_world_{};
    Vec2 mouse_press_px_{};
    std::vector<u32> drag_ids_{};
    std::vector<Vec2> drag_initial_positions_{};
    std::optional<LegTarget> rotating_leg_{};
    Vec2 box_min_world_{};
    Vec2 box_max_world_{};

    bool radial_open_{};
    Vec2 radial_center_px_{};
    i32 radial_hovered_{-1};

    std::string status_message_{};
    f32 status_until_{};
    f32 elapsed_{};

    auto set_status(std::string message, f32 duration_seconds = 6.0f) -> void
    {
        status_message_ = std::move(message);
        status_until_ = elapsed_ + duration_seconds;
        std::cout << "[viz] " << status_message_ << '\n';
    }

    // ------------------------------------------------------------------
    // Scene seeding
    // ------------------------------------------------------------------

    auto seed_demo_scene() -> void
    {
        viz::IdCounters counters{.next_id = 1, .next_leg_id = 1, .next_bond_id = 1, .next_abs_id = 1};
        auto mps = viz::build_mps(counters, 6, 4, 2, Vec2{0.0f, 0.0f});
        auto mpo = viz::build_mpo(counters, 6, 3, 2, Vec2{0.0f, 200.0f});
        Frame f;
        f.name = "demo MPS + MPO";
        f.source = "(built-in template)";
        std::vector<Tensor> tensors;
        std::vector<Bond> bonds;
        std::vector<Abstraction> abstractions;
        for (auto& t : mps.tensors)
        {
            tensors.push_back(std::move(t));
        }
        for (auto& b : mps.bonds)
        {
            bonds.push_back(std::move(b));
        }
        abstractions.push_back(std::move(mps.abstraction));
        for (auto& t : mpo.tensors)
        {
            tensors.push_back(std::move(t));
        }
        for (auto& b : mpo.bonds)
        {
            bonds.push_back(std::move(b));
        }
        abstractions.push_back(std::move(mpo.abstraction));
        viz::frame_finalize(f, std::move(tensors), std::move(bonds), std::move(abstractions));
        f.next_id = counters.next_id;
        f.next_leg_id = counters.next_leg_id;
        f.next_bond_id = counters.next_bond_id;
        f.next_abs_id = counters.next_abs_id;
        frames_.push_back(std::move(f));
        frame_source_ = "demo";
    }

    auto load_path(const std::filesystem::path& path) -> void
    {
        try
        {
            const Vec2 center{0.0f, 0.0f};
            auto loaded = viz::load_envelope_folder(path, center);
            if (loaded.frames.empty())
            {
                set_status("no frames found in " + path.string());
                return;
            }
            frames_ = std::move(loaded.frames);
            frame_source_ = std::move(loaded.source);
            set_status(
                "loaded " + std::to_string(frames_.size()) + " frame(s) from "
                + path.string()
            );
        }
        catch (const std::exception& e)
        {
            set_status(std::string{"load failed: "} + e.what(), 10.0f);
        }
    }

    auto apply_frame(usize index) -> void
    {
        if (frames_.empty())
        {
            return;
        }
        current_frame_ = std::min(index, frames_.size() - 1);
        clear_selection();
        hovered_tensor_.reset();
        hovered_leg_.reset();
        hovered_abstraction_.reset();
        mouse_action_ = MouseAction::idle;
    }

    [[nodiscard]] auto scene_center() const noexcept -> Vec2
    {
        const auto& f = live();
        if (f.tensors.empty())
        {
            return Vec2{0.0f, 0.0f};
        }
        f32 sx = 0.0f;
        f32 sy = 0.0f;
        for (const auto& t : f.tensors)
        {
            sx += t.x;
            sy += t.y;
        }
        const auto n = static_cast<f32>(f.tensors.size());
        return Vec2{sx / n, sy / n};
    }

    // ------------------------------------------------------------------
    // Selection helpers
    // ------------------------------------------------------------------

    [[nodiscard]] auto is_standalone_selected(u32 id) const noexcept -> bool
    {
        return std::find(selected_tensors_.begin(), selected_tensors_.end(), id)
               != selected_tensors_.end();
    }

    [[nodiscard]] auto is_abs_selected(u32 abs_id) const noexcept -> bool
    {
        return std::find(selected_abstractions_.begin(), selected_abstractions_.end(), abs_id)
               != selected_abstractions_.end();
    }

    // Tensor highlight: standalone-selected, or member of a selected container,
    // or the drilled-into inner_sel_.
    [[nodiscard]] auto is_selected(u32 id) const noexcept -> bool
    {
        if (is_standalone_selected(id))
        {
            return true;
        }
        if (inner_sel_.has_value() and *inner_sel_ == id)
        {
            return true;
        }
        const auto* t = find_tensor(live(), id);
        if (t == nullptr or not t->abs_id.has_value())
        {
            return false;
        }
        return is_abs_selected(*t->abs_id);
    }

    auto toggle_standalone(u32 id) -> void
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

    auto toggle_abstraction(u32 id) -> void
    {
        const auto it = std::find(
            selected_abstractions_.begin(), selected_abstractions_.end(), id
        );
        if (it == selected_abstractions_.end())
        {
            selected_abstractions_.push_back(id);
        }
        else
        {
            selected_abstractions_.erase(it);
        }
    }

    auto clear_selection() -> void
    {
        selected_tensors_.clear();
        selected_abstractions_.clear();
        inner_sel_.reset();
    }

    // Union of standalone-selected tensor ids and members of selected
    // abstractions; used by drag-move and Delete.
    [[nodiscard]] auto selection_tensor_set() const -> std::vector<u32>
    {
        std::vector<u32> out;
        std::unordered_set<u32> seen;
        for (const auto id : selected_tensors_)
        {
            if (seen.insert(id).second)
            {
                out.push_back(id);
            }
        }
        for (const auto abs_id : selected_abstractions_)
        {
            for (const auto& a : live().abstractions)
            {
                if (a.id != abs_id)
                {
                    continue;
                }
                for (const auto m : a.members)
                {
                    if (seen.insert(m).second)
                    {
                        out.push_back(m);
                    }
                }
            }
        }
        return out;
    }

    [[nodiscard]] auto abstraction_bbox(const Abstraction& a) const
        -> std::optional<std::array<f32, 4>>
    {
        if (a.members.empty())
        {
            return std::nullopt;
        }
        std::optional<f32> mn_x;
        f32 mx_x = 0.0f;
        f32 mn_y = 0.0f;
        f32 mx_y = 0.0f;
        f32 max_r = 0.0f;
        for (const auto id : a.members)
        {
            const auto* t = find_tensor(live(), id);
            if (t == nullptr)
            {
                continue;
            }
            const auto r = node_radius(t->rank);
            max_r = std::max(max_r, r);
            if (not mn_x.has_value())
            {
                mn_x = t->x;
                mx_x = t->x;
                mn_y = t->y;
                mx_y = t->y;
            }
            else
            {
                mn_x = std::min(*mn_x, t->x);
                mx_x = std::max(mx_x, t->x);
                mn_y = std::min(mn_y, t->y);
                mx_y = std::max(mx_y, t->y);
            }
        }
        if (not mn_x.has_value())
        {
            return std::nullopt;
        }
        const auto pad = max_r + 40.0f;
        return std::array<f32, 4>{*mn_x - pad, mx_x + pad, mn_y - pad, mx_y + pad};
    }

    auto seed_drag_positions() -> void
    {
        drag_ids_ = selection_tensor_set();
        drag_initial_positions_.clear();
        drag_initial_positions_.reserve(drag_ids_.size());
        for (const auto id : drag_ids_)
        {
            if (const auto* t = find_tensor(live(), id); t != nullptr)
            {
                drag_initial_positions_.push_back(Vec2{t->x, t->y});
            }
            else
            {
                drag_initial_positions_.push_back(Vec2{0.0f, 0.0f});
            }
        }
    }

    [[nodiscard]] auto hit_abstraction(Vec2 world_pos) const -> std::optional<u32>
    {
        for (auto it = live().abstractions.rbegin(); it != live().abstractions.rend(); ++it)
        {
            const auto bb = abstraction_bbox(*it);
            if (not bb.has_value())
            {
                continue;
            }
            if (world_pos.x >= (*bb)[0] and world_pos.x <= (*bb)[1]
                and world_pos.y >= (*bb)[2] and world_pos.y <= (*bb)[3])
            {
                return it->id;
            }
        }
        return std::nullopt;
    }

    // ------------------------------------------------------------------
    // Hit testing
    // ------------------------------------------------------------------

    [[nodiscard]] auto hit_tensor(Vec2 world_pos) const -> std::optional<u32>
    {
        const auto& f = live();
        for (auto it = f.tensors.rbegin(); it != f.tensors.rend(); ++it)
        {
            const auto r = node_radius(it->rank);
            if (std::abs(world_pos.x - it->x) <= r and std::abs(world_pos.y - it->y) <= r)
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
        const auto& f = live();
        for (const auto& t : f.tensors)
        {
            for (i32 leg_i = 0; leg_i < t.rank; ++leg_i)
            {
                if (is_leg_bonded(f, t.id, leg_i))
                {
                    continue;
                }
                const auto tip = leg_tip(f, t, leg_i);
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
            hovered_abstraction_ = std::nullopt;
            return;
        }
        hovered_leg_ = hit_leg_tip(mouse_world);
        hovered_tensor_ = hovered_leg_.has_value() ? std::nullopt : hit_tensor(mouse_world);
        hovered_abstraction_
            = (hovered_leg_.has_value() or hovered_tensor_.has_value())
                  ? std::nullopt
                  : hit_abstraction(mouse_world);

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
            if (input.left_click.occurred or input.right_click.occurred)
            {
                close_radial(true);
            }
            return;
        }

        if (input.right_click.occurred and not input.mouse_captured_by_ui)
        {
            open_radial(input.mouse_px);
            return;
        }

        if (input.left_click.occurred and not input.mouse_captured_by_ui)
        {
            mouse_press_world_ = mouse_world;
            mouse_press_px_ = input.mouse_px;
            mouse_action_ = MouseAction::pressed;
            const auto shift = input.left_click.modifiers.shift;

            if (hovered_leg_.has_value())
            {
                rotating_leg_ = hovered_leg_;
                if (not shift)
                {
                    clear_selection();
                    selected_tensors_.push_back(rotating_leg_->tensor);
                }
            }
            else if (hovered_tensor_.has_value())
            {
                const auto id = *hovered_tensor_;
                const auto* t = find_tensor(live(), id);
                const auto abs_id = (t != nullptr) ? t->abs_id : std::optional<u32>{};
                if (abs_id.has_value())
                {
                    if (shift)
                    {
                        toggle_abstraction(*abs_id);
                        inner_sel_.reset();
                    }
                    else if (is_abs_selected(*abs_id))
                    {
                        inner_sel_ = id;
                    }
                    else
                    {
                        clear_selection();
                        selected_abstractions_.push_back(*abs_id);
                    }
                }
                else
                {
                    if (shift)
                    {
                        toggle_standalone(id);
                    }
                    else if (not is_standalone_selected(id))
                    {
                        clear_selection();
                        selected_tensors_.push_back(id);
                    }
                }
                seed_drag_positions();
            }
            else if (const auto hit_abs = hit_abstraction(mouse_world); hit_abs.has_value())
            {
                if (shift)
                {
                    toggle_abstraction(*hit_abs);
                }
                else if (not is_abs_selected(*hit_abs))
                {
                    clear_selection();
                    selected_abstractions_.push_back(*hit_abs);
                }
                inner_sel_.reset();
                seed_drag_positions();
            }
            else
            {
                if (not shift)
                {
                    clear_selection();
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
                    push_undo();
                    mouse_action_ = MouseAction::rotating_leg;
                }
                else if (hovered_tensor_.has_value() or hovered_abstraction_.has_value()
                         or not drag_ids_.empty())
                {
                    push_undo();
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
            auto* t = find_tensor(live(), rotating_leg_->tensor);
            if (t != nullptr)
            {
                const Vec2 v{mouse_world.x - t->x, mouse_world.y - t->y};
                if (length2(v) > 1.0f)
                {
                    if (rotating_leg_->leg_index >= 0
                        and static_cast<usize>(rotating_leg_->leg_index) < t->leg_angles.size())
                    {
                        t->leg_angles[static_cast<usize>(rotating_leg_->leg_index)]
                            = std::atan2(v.y, v.x);
                    }
                }
            }
        }
        else if (mouse_action_ == MouseAction::dragging_tensors)
        {
            const auto world_delta = mouse_world - mouse_press_world_;
            for (auto i = 0u; i < drag_ids_.size() and i < drag_initial_positions_.size(); ++i)
            {
                auto* t = find_tensor(live(), drag_ids_[i]);
                if (t != nullptr)
                {
                    t->x = drag_initial_positions_[i].x + world_delta.x;
                    t->y = drag_initial_positions_[i].y + world_delta.y;
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
            clear_selection();
        }
        for (const auto& t : live().tensors)
        {
            // Tensors that live inside a container don't get picked up as
            // standalone selections; selecting a container is done by clicking
            // it explicitly.
            if (t.abs_id.has_value())
            {
                continue;
            }
            if (t.x >= box_min_world_.x and t.x <= box_max_world_.x and t.y >= box_min_world_.y
                and t.y <= box_max_world_.y)
            {
                if (not is_standalone_selected(t.id))
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
        if (input.key_left_pressed and current_frame_ > 0)
        {
            apply_frame(current_frame_ - 1);
            set_status(
                "frame " + std::to_string(current_frame_ + 1) + "/"
                + std::to_string(frames_.size()) + ": " + live().name
            );
        }
        if (input.key_right_pressed and current_frame_ + 1 < frames_.size())
        {
            apply_frame(current_frame_ + 1);
            set_status(
                "frame " + std::to_string(current_frame_ + 1) + "/"
                + std::to_string(frames_.size()) + ": " + live().name
            );
        }
        if (input.key_c_pressed and not radial_open_)
        {
            try_contract();
        }
        if (input.key_m_pressed)
        {
            if (input.shift_held)
            {
                merge_into_abstraction("MPS");
            }
            else
            {
                insert_template_mps(mouse_world);
            }
        }
        if (input.key_o_pressed)
        {
            if (input.shift_held)
            {
                merge_into_abstraction("MPO");
            }
            else
            {
                insert_template_mpo(mouse_world);
            }
        }
        if (input.key_p_pressed)
        {
            insert_template_peps(mouse_world);
        }
        if (input.key_n_pressed and not radial_open_)
        {
            insert_tensor(mouse_world);
        }
        if (input.key_delete_pressed)
        {
            delete_selected();
        }
        if (input.key_z_pressed and (input.super_held or input.control_held))
        {
            if (input.shift_held)
            {
                redo();
            }
            else
            {
                undo();
            }
        }
        if (input.key_plus_pressed and hovered_leg_.has_value())
        {
            push_undo();
            adjust_leg_prime(hovered_leg_->tensor, hovered_leg_->leg_index, +1);
        }
        if (input.key_minus_pressed and hovered_leg_.has_value())
        {
            push_undo();
            adjust_leg_prime(hovered_leg_->tensor, hovered_leg_->leg_index, -1);
        }
    }

    auto adjust_leg_prime(u32 tid, i32 leg_index, i32 delta) -> void
    {
        auto* t = find_tensor(live(), tid);
        if (t == nullptr or leg_index < 0 or static_cast<usize>(leg_index) >= t->leg_prime.size())
        {
            return;
        }
        auto& v = t->leg_prime[static_cast<usize>(leg_index)];
        v = std::max(0, v + delta);
        if (static_cast<usize>(leg_index) < t->src_inds.size())
        {
            t->src_inds[static_cast<usize>(leg_index)].plev = v;
        }
    }

    auto insert_tensor(Vec2 world_pos) -> void
    {
        push_undo();
        auto& f = live();
        Tensor t;
        t.id = f.next_id++;
        t.label = std::format("T{}", t.id);
        t.x = world_pos.x;
        t.y = world_pos.y;
        t.rank = 3;
        t.dims = {2, 2, 2};
        t.leg_angles = viz::default_leg_angles(t.rank);
        t.leg_prime.assign(static_cast<usize>(t.rank), 0);
        t.leg_id.reserve(static_cast<usize>(t.rank));
        for (i32 i = 0; i < t.rank; ++i)
        {
            t.leg_id.push_back(f.next_leg_id++);
        }
        t.color = viz::rank_color(t.rank);
        t.src_inds.resize(static_cast<usize>(t.rank));
        const auto id = t.id;
        f.tensors.push_back(std::move(t));
        selected_tensors_ = {id};
        set_status("added tensor T" + std::to_string(id));
    }

    auto delete_selected() -> void
    {
        const auto ids = selection_tensor_set();
        if (ids.empty())
        {
            return;
        }
        push_undo();
        auto& f = live();
        for (const auto id : ids)
        {
            std::erase_if(f.bonds, [id](const Bond& b) {
                return b.a == id or b.b == id;
            });
            for (auto& a : f.abstractions)
            {
                std::erase(a.members, id);
            }
            std::erase_if(f.tensors, [id](const Tensor& t) { return t.id == id; });
        }
        std::erase_if(f.abstractions, [](const Abstraction& a) { return a.members.empty(); });
        const auto n = ids.size();
        clear_selection();
        set_status("deleted " + std::to_string(n) + " tensor(s)");
    }

    // Undo/redo: snapshot the live frame before each mutation. Stack is bounded.
    static constexpr usize k_undo_limit = 64;
    std::vector<Frame> undo_stack_{};
    std::vector<Frame> redo_stack_{};

    auto push_undo() -> void
    {
        undo_stack_.push_back(live());
        if (undo_stack_.size() > k_undo_limit)
        {
            undo_stack_.erase(undo_stack_.begin());
        }
        redo_stack_.clear();
    }

    auto undo() -> void
    {
        if (undo_stack_.empty())
        {
            set_status("nothing to undo");
            return;
        }
        redo_stack_.push_back(live());
        live() = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        clear_selection();
        set_status("undo");
    }

    auto redo() -> void
    {
        if (redo_stack_.empty())
        {
            set_status("nothing to redo");
            return;
        }
        undo_stack_.push_back(live());
        live() = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        clear_selection();
        set_status("redo");
    }

    struct MergeAnalysis
    {
        bool mps_ok{};
        bool mpo_ok{};
        std::vector<u32> order{};
    };

    [[nodiscard]] auto analyze_merge(const std::vector<u32>& ids) const -> MergeAnalysis
    {
        MergeAnalysis result;
        if (ids.size() < 2)
        {
            return result;
        }
        const auto& f = live();
        std::unordered_map<u32, std::vector<u32>> adj;
        std::unordered_map<u32, i32> deg;
        std::unordered_set<u32> inset(ids.begin(), ids.end());
        for (const auto id : ids)
        {
            deg[id] = 0;
            adj[id] = {};
        }
        for (const auto& b : f.bonds)
        {
            if (inset.contains(b.a) and inset.contains(b.b))
            {
                deg[b.a] += 1;
                deg[b.b] += 1;
                adj[b.a].push_back(b.b);
                adj[b.b].push_back(b.a);
            }
        }
        i32 ends = 0;
        bool bad = false;
        for (const auto id : ids)
        {
            const auto d = deg[id];
            if (d == 1)
            {
                ++ends;
            }
            else if (d != 2)
            {
                bad = true;
            }
        }
        std::unordered_set<u32> seen;
        seen.insert(ids[0]);
        std::vector<u32> stack{ids[0]};
        while (not stack.empty())
        {
            const auto u = stack.back();
            stack.pop_back();
            for (const auto v : adj[u])
            {
                if (seen.insert(v).second)
                {
                    stack.push_back(v);
                }
            }
        }
        const bool is_chain = not bad and ends == 2 and seen.size() == ids.size();
        result.mps_ok = is_chain;
        result.mpo_ok = is_chain;
        for (const auto id : ids)
        {
            const auto* t = find_tensor(live(), id);
            if (t == nullptr)
            {
                result.mps_ok = false;
                result.mpo_ok = false;
                continue;
            }
            const auto free = t->rank - deg[id];
            if (free != 1)
            {
                result.mps_ok = false;
            }
            if (free != 2)
            {
                result.mpo_ok = false;
            }
        }
        if (is_chain)
        {
            u32 start = ids[0];
            for (const auto id : ids)
            {
                if (deg[id] == 1)
                {
                    start = id;
                    break;
                }
            }
            std::unordered_set<u32> vis{start};
            std::vector<u32> order{start};
            auto cur = start;
            while (order.size() < ids.size())
            {
                u32 next_id = 0;
                bool found = false;
                for (const auto v : adj[cur])
                {
                    if (not vis.contains(v))
                    {
                        next_id = v;
                        found = true;
                        break;
                    }
                }
                if (not found)
                {
                    break;
                }
                vis.insert(next_id);
                order.push_back(next_id);
                cur = next_id;
            }
            if (order.size() == ids.size())
            {
                result.order = std::move(order);
            }
        }
        return result;
    }

    auto merge_into_abstraction(std::string_view type_name) -> void
    {
        if (selected_tensors_.size() < 2)
        {
            set_status("select 2+ tensors forming a chain to merge");
            return;
        }
        const auto info = analyze_merge(selected_tensors_);
        const auto ok = (type_name == "MPS") ? info.mps_ok : info.mpo_ok;
        if (not ok or info.order.empty())
        {
            set_status(
                std::string{"selection is not a valid "} + std::string{type_name} + " chain"
            );
            return;
        }
        push_undo();
        auto& f = live();
        Abstraction abs;
        abs.id = f.next_abs_id++;
        abs.type_name = std::string{type_name};
        abs.name = std::string{type_name} + std::to_string(abs.id);
        abs.members = info.order;
        const auto abs_id = abs.id;
        for (const auto id : info.order)
        {
            if (auto* t = find_tensor(f, id); t != nullptr)
            {
                t->abs_id = abs_id;
            }
        }
        f.abstractions.push_back(std::move(abs));
        set_status("merged " + std::to_string(info.order.size()) + " tensors -> " + std::string{type_name});
    }

    auto insert_template_mps(Vec2 center) -> void
    {
        push_undo();
        auto& f = live();
        viz::IdCounters c{
            .next_id = f.next_id,
            .next_leg_id = f.next_leg_id,
            .next_bond_id = f.next_bond_id,
            .next_abs_id = f.next_abs_id,
        };
        auto out = viz::build_mps(c, 6, 4, 2, center);
        absorb_template(out, c);
        set_status("inserted MPS(6)");
    }

    auto insert_template_mpo(Vec2 center) -> void
    {
        push_undo();
        auto& f = live();
        viz::IdCounters c{
            .next_id = f.next_id,
            .next_leg_id = f.next_leg_id,
            .next_bond_id = f.next_bond_id,
            .next_abs_id = f.next_abs_id,
        };
        auto out = viz::build_mpo(c, 6, 3, 2, center);
        absorb_template(out, c);
        set_status("inserted MPO(6)");
    }

    auto insert_template_peps(Vec2 center) -> void
    {
        push_undo();
        auto& f = live();
        viz::IdCounters c{
            .next_id = f.next_id,
            .next_leg_id = f.next_leg_id,
            .next_bond_id = f.next_bond_id,
            .next_abs_id = f.next_abs_id,
        };
        auto out = viz::build_peps(c, 3, 3, 3, 2, center);
        absorb_template(out, c);
        set_status("inserted PEPS(3x3)");
    }

    auto absorb_template(viz::TemplateOutput& out, viz::IdCounters c) -> void
    {
        auto& f = live();
        for (auto& t : out.tensors)
        {
            f.tensors.push_back(std::move(t));
        }
        for (auto& b : out.bonds)
        {
            f.bonds.push_back(std::move(b));
        }
        f.abstractions.push_back(std::move(out.abstraction));
        f.next_id = c.next_id;
        f.next_leg_id = c.next_leg_id;
        f.next_bond_id = c.next_bond_id;
        f.next_abs_id = c.next_abs_id;
    }

    auto try_contract() -> void
    {
        if (not julia_.has_value())
        {
            set_status("Julia client disabled (use without --no-julia)");
            return;
        }
        if (selected_abstractions_.size() != 2)
        {
            set_status(
                "select two abstractions to contract (got "
                + std::to_string(selected_abstractions_.size()) + ")"
            );
            return;
        }
        const Abstraction* left = nullptr;
        const Abstraction* right = nullptr;
        for (const auto& a : live().abstractions)
        {
            if (a.id == selected_abstractions_[0])
            {
                left = &a;
            }
            else if (a.id == selected_abstractions_[1])
            {
                right = &a;
            }
        }
        if (left == nullptr or right == nullptr)
        {
            set_status("abstraction lookup failed");
            return;
        }
        const Vec2 stage_center{scene_center().x, scene_center().y};
        auto result = julia_->contract(*left, *right, live().tensors, "lr", stage_center);
        if (not result.ok)
        {
            set_status("contract failed: " + result.error, 10.0f);
            return;
        }
        push_undo();
        merge_frame_into_live(result.frame, left->name + " x " + right->name);
        set_status("contracted " + left->name + " x " + right->name);
    }

    auto merge_frame_into_live(Frame& incoming, std::string new_name) -> void
    {
        auto& f = live();
        const auto tid_off = f.next_id - 1;
        const auto leg_off = f.next_leg_id - 1;
        const auto bid_off = f.next_bond_id - 1;
        const auto aid_off = f.next_abs_id - 1;

        // Shift incoming ids
        std::map<u32, u32> id_map;
        for (auto& t : incoming.tensors)
        {
            const auto new_id = t.id + tid_off;
            id_map[t.id] = new_id;
            t.id = new_id;
            for (auto& l : t.leg_id)
            {
                l += leg_off;
            }
            if (t.abs_id.has_value())
            {
                t.abs_id = *t.abs_id + aid_off;
            }
        }
        for (auto& b : incoming.bonds)
        {
            b.id += bid_off;
            b.a = id_map[b.a];
            b.b = id_map[b.b];
        }
        for (auto& a : incoming.abstractions)
        {
            a.id += aid_off;
            for (auto& m : a.members)
            {
                m = id_map[m];
            }
        }
        if (not incoming.abstractions.empty())
        {
            incoming.abstractions.front().name = std::move(new_name);
        }
        // Position below the operands
        f32 sum_y = 0.0f;
        usize count = 0;
        for (const auto& t : f.tensors)
        {
            sum_y += t.y;
            ++count;
        }
        const auto target_y = (count > 0) ? sum_y / static_cast<f32>(count) + 260.0f : 0.0f;
        f32 in_sum_x = 0.0f;
        f32 in_sum_y = 0.0f;
        for (const auto& t : incoming.tensors)
        {
            in_sum_x += t.x;
            in_sum_y += t.y;
        }
        const auto in_count = static_cast<f32>(incoming.tensors.size());
        if (in_count > 0.0f)
        {
            const auto dx = -in_sum_x / in_count;
            const auto dy = target_y - in_sum_y / in_count;
            for (auto& t : incoming.tensors)
            {
                t.x += dx;
                t.y += dy;
            }
        }

        for (auto& t : incoming.tensors)
        {
            f.tensors.push_back(std::move(t));
        }
        for (auto& b : incoming.bonds)
        {
            f.bonds.push_back(std::move(b));
        }
        for (auto& a : incoming.abstractions)
        {
            f.abstractions.push_back(std::move(a));
        }
        f.next_id += incoming.next_id;
        f.next_leg_id += incoming.next_leg_id;
        f.next_bond_id += incoming.next_bond_id;
        f.next_abs_id += incoming.next_abs_id;
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
            const auto label = items[static_cast<usize>(radial_hovered_)].label;
            if (label == "contract")
            {
                try_contract();
            }
            else if (label == "delete")
            {
                delete_selected();
            }
            else
            {
                set_status("menu action: " + label + " (not wired yet)");
            }
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
        const auto sector
            = static_cast<i32>(angle * static_cast<f32>(k_radial_outer_count) / two_pi);
        return std::clamp(sector, 0, k_radial_outer_count - 1);
    }

    // ------------------------------------------------------------------
    // Bond layout
    // ------------------------------------------------------------------

    [[nodiscard]] auto compute_bond_lanes() const -> std::map<u32, std::pair<i32, i32>>
    {
        struct LaneKey
        {
            u32 lo, hi;
            bool operator<(const LaneKey& o) const noexcept
            {
                return std::tie(lo, hi) < std::tie(o.lo, o.hi);
            }
        };
        std::map<LaneKey, std::vector<u32>> groups;
        for (const auto& b : live().bonds)
        {
            const auto lo = std::min(b.a, b.b);
            const auto hi = std::max(b.a, b.b);
            groups[LaneKey{lo, hi}].push_back(b.id);
        }
        std::map<u32, std::pair<i32, i32>> result;
        for (const auto& [_, ids] : groups)
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
        const auto half_w = 0.5f * logical.x * zoom;
        const auto half_h = 0.5f * logical.y * zoom;

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
        for (const auto& a : live().abstractions)
        {
            if (a.members.empty())
            {
                continue;
            }
            std::optional<Vec2> mn;
            std::optional<Vec2> mx;
            f32 max_radius = 0.0f;
            for (const auto id : a.members)
            {
                const auto* t = find_tensor(live(), id);
                if (t == nullptr)
                {
                    continue;
                }
                const auto r = node_radius(t->rank);
                max_radius = std::max(max_radius, r);
                const Vec2 p{t->x, t->y};
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
            const auto pad = max_radius + 40.0f;
            const auto top_left = Vec2{mn->x - pad, mn->y - pad};
            const auto size = Vec2{(mx->x - mn->x) + 2.0f * pad, (mx->y - mn->y) + 2.0f * pad};
            const auto selected = is_abs_selected(a.id);
            const auto hovered
                = hovered_abstraction_.has_value() and *hovered_abstraction_ == a.id;
            Color fill{0.42f, 0.58f, 0.82f, 0.05f};
            Color stroke{0.58f, 0.74f, 0.96f, 0.55f};
            f32 stroke_w = 1.6f;
            if (selected)
            {
                fill = Color{0.46f, 0.66f, 0.95f, 0.15f};
                stroke = Color{1.0f, 1.0f, 1.0f, 0.95f};
                stroke_w = 2.6f;
            }
            else if (hovered)
            {
                fill = Color{0.46f, 0.66f, 0.95f, 0.08f};
                stroke = Color{0.85f, 0.92f, 1.0f, 0.80f};
                stroke_w = 2.0f;
            }
            frame.draw.rect(
                {
                    .position = top_left,
                    .size = size,
                    .fill_color = fill,
                    .stroke_color = stroke,
                    .stroke_width = stroke_w,
                    .corner_radius = 28.0f,
                }
            );
            frame.draw.text(
                {
                    .position = Vec2{top_left.x + 16.0f, top_left.y + 24.0f},
                    .text = a.name + " (" + std::to_string(a.members.size()) + ")" + a.env_suffix,
                    .color = Color{0.84f, 0.92f, 1.0f, 0.92f},
                    .size_scale = 0.55f,
                }
            );
        }
    }

    auto draw_bonds(FrameContext& frame) -> void
    {
        const auto lanes = compute_bond_lanes();
        for (const auto& bond : live().bonds)
        {
            const auto* a = find_tensor(live(), bond.a);
            const auto* b = find_tensor(live(), bond.b);
            if (a == nullptr or b == nullptr)
            {
                continue;
            }
            const Vec2 from{a->x, a->y};
            const Vec2 to{b->x, b->y};
            const auto along = to - from;
            const Vec2 mid{0.5f * (from.x + to.x), 0.5f * (from.y + to.y)};
            const auto along_norm = normalize_or(along, Vec2{1.0f, 0.0f});
            const auto perp_norm = perp(along_norm);

            const auto it = lanes.find(bond.id);
            const auto [lane_index, total]
                = (it != lanes.end()) ? it->second : std::pair<i32, i32>{0, 1};
            const auto half = static_cast<f32>(total - 1) * 0.5f;
            const auto bulge_amount
                = (total <= 1) ? 0.0f : k_bond_lane_offset * (static_cast<f32>(lane_index) - half);
            const Vec2 control{
                mid.x + perp_norm.x * bulge_amount,
                mid.y + perp_norm.y * bulge_amount,
            };

            // Tags and prime come from the (a, ai) leg of the source tensor;
            // all tag strings on that leg show in the bond label, prime ticks
            // append to the last line (matching the HTML behaviour).
            std::vector<std::string> bond_tags;
            i32 prime_level = 0;
            if (bond.ai >= 0 and static_cast<usize>(bond.ai) < a->src_inds.size())
            {
                const auto& s = a->src_inds[static_cast<usize>(bond.ai)];
                bond_tags = s.tags;
                prime_level = s.plev;
            }
            const auto primary_tag = bond_tags.empty() ? std::string{} : bond_tags.front();
            const auto color = viz::tag_color(primary_tag);
            const auto dashed = (prime_level > 0);
            frame.draw.bezier(
                {
                    .start = from,
                    .control = control,
                    .end = to,
                    .color = color,
                    .thickness = k_bond_thickness,
                    .dash_on = dashed ? 9.0f : 0.0f,
                    .dash_off = dashed ? 6.0f : 0.0f,
                    .segments = 28u,
                }
            );

            if (not bond_tags.empty() or prime_level > 0)
            {
                const auto t_label = (lane_index % 2 == 0) ? 0.35f : 0.65f;
                const auto label_pos = quad_bezier(from, control, to, t_label);
                draw_bond_label(frame, label_pos, bond_tags, prime_level, color);
            }
        }
    }

    auto draw_bond_label(
        FrameContext& frame,
        Vec2 world_pos,
        const std::vector<std::string>& tags,
        i32 prime,
        Color base
    ) -> void
    {
        std::vector<std::string> lines = tags;
        if (prime > 0)
        {
            std::string ticks;
            if (prime <= 3)
            {
                ticks.assign(static_cast<usize>(prime), '\'');
            }
            else
            {
                ticks = std::to_string(prime) + "'";
            }
            if (lines.empty())
            {
                lines.push_back(ticks);
            }
            else
            {
                lines.back() += ticks;
            }
        }
        if (lines.empty())
        {
            return;
        }
        const auto glyph_w = 7.0f;
        f32 longest = 0.0f;
        for (const auto& line : lines)
        {
            longest = std::max(longest, static_cast<f32>(line.size()) * glyph_w);
        }
        const auto pad_x = 5.0f;
        const auto pad_y = 3.0f;
        const auto line_h = 12.0f;
        const auto w = longest + 2.0f * pad_x;
        const auto h = line_h * static_cast<f32>(lines.size()) + 2.0f * pad_y;
        const auto top_left = Vec2{world_pos.x - w * 0.5f, world_pos.y - h * 0.5f};
        frame.draw.rect(
            {
                .position = top_left,
                .size = Vec2{w, h},
                .fill_color = Color{0.06f, 0.10f, 0.14f, 0.86f},
                .stroke_color = with_alpha(base, 0.65f),
                .stroke_width = 1.0f,
                .corner_radius = 5.0f,
            }
        );
        for (usize i = 0; i < lines.size(); ++i)
        {
            frame.draw.text(
                {
                    .position = Vec2{
                        top_left.x + pad_x,
                        top_left.y + pad_y + line_h * (static_cast<f32>(i) + 0.7f),
                    },
                    .text = lines[i],
                    .color = with_alpha(base, 0.95f),
                    .size_scale = 0.42f,
                }
            );
        }
    }

    auto draw_free_legs(FrameContext& frame) -> void
    {
        const auto& f = live();
        for (const auto& t : f.tensors)
        {
            for (i32 leg_i = 0; leg_i < t.rank; ++leg_i)
            {
                if (is_leg_bonded(f, t.id, leg_i))
                {
                    continue;
                }
                std::string primary_tag;
                i32 prime = 0;
                std::vector<std::string> tag_list;
                if (static_cast<usize>(leg_i) < t.src_inds.size())
                {
                    const auto& s = t.src_inds[static_cast<usize>(leg_i)];
                    tag_list = s.tags;
                    prime = s.plev;
                    if (not s.tags.empty())
                    {
                        primary_tag = s.tags.front();
                    }
                }
                const auto color = viz::tag_color(primary_tag);
                const auto anchor = leg_anchor(f, t, leg_i);
                const auto tip = Vec2{
                    anchor.point.x + anchor.normal.x * k_leg_length,
                    anchor.point.y + anchor.normal.y * k_leg_length,
                };
                const auto dashed = (prime > 0);

                frame.draw.line_2d(
                    {
                        .start = anchor.point,
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

                draw_leg_label(frame, anchor.normal, tip, color, tag_list, prime);
            }
        }
    }

    auto draw_leg_label(
        FrameContext& frame,
        Vec2 normal,
        Vec2 tip,
        Color color,
        const std::vector<std::string>& tags,
        i32 prime
    ) -> void
    {
        std::vector<std::string> lines = tags;
        if (prime > 0)
        {
            std::string ticks;
            if (prime <= 3)
            {
                ticks.assign(static_cast<usize>(prime), '\'');
            }
            else
            {
                ticks = std::to_string(prime) + "'";
            }
            if (lines.empty())
            {
                lines.push_back(ticks);
            }
            else
            {
                lines.back() += ticks;
            }
        }
        if (lines.empty())
        {
            return;
        }
        // Place the label off the tip along the normal; stack direction depends
        // on which way the leg points (matches drawFreeLegs in the HTML).
        const auto base_x = tip.x + normal.x * 7.0f;
        const auto base_y = tip.y + normal.y * 7.0f;
        constexpr f32 glyph_w = 7.0f;
        constexpr f32 line_h = 11.0f;
        f32 max_w = 0.0f;
        for (const auto& line : lines)
        {
            max_w = std::max(max_w, static_cast<f32>(line.size()) * glyph_w);
        }
        const auto stack_count = static_cast<f32>(lines.size());
        for (usize i = 0; i < lines.size(); ++i)
        {
            const auto line_w = static_cast<f32>(lines[i].size()) * glyph_w;
            f32 x = base_x;
            f32 y = base_y;
            if (normal.y < -0.5f)
            {
                // pointing up: stack lines upward (last line nearest the tip)
                x -= line_w * 0.5f;
                y -= (stack_count - 1.0f - static_cast<f32>(i)) * line_h;
            }
            else if (normal.y > 0.5f)
            {
                // pointing down: stack lines downward
                x -= line_w * 0.5f;
                y += static_cast<f32>(i) * line_h + 8.0f;
            }
            else
            {
                // horizontal: align based on normal.x sign
                if (normal.x > 0.0f)
                {
                    // right-pointing leg
                }
                else
                {
                    x -= max_w;
                }
                y += (static_cast<f32>(i) - (stack_count - 1.0f) * 0.5f) * line_h + 4.0f;
            }
            const Vec2 pos{x, y};
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
        for (const auto& t : live().tensors)
        {
            const auto r = node_radius(t.rank);
            const auto selected = is_selected(t.id);
            const auto hovered = (hovered_tensor_.has_value() and *hovered_tensor_ == t.id);

            if (selected)
            {
                frame.draw.rect(
                    {
                        .position = Vec2{t.x - r - 4.0f, t.y - r - 4.0f},
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
                    .position = Vec2{t.x - r, t.y - r},
                    .size = Vec2{2.0f * r, 2.0f * r},
                    .fill_color = t.color,
                    .stroke_color = stroke,
                    .stroke_width = stroke_w,
                    .corner_radius = std::max(4.0f, r * k_node_bevel_ratio),
                }
            );

            const auto label_width_approx = static_cast<f32>(t.label.size()) * 8.0f;
            frame.draw.text(
                {
                    .position = Vec2{t.x - label_width_approx * 0.5f, t.y + 5.0f},
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
            const auto a1
                = two_pi * static_cast<f32>(i + 1) / static_cast<f32>(k_radial_outer_count);
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
            const Vec2 label_pos{
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
                .position = Vec2{24.0f, 30.0f},
                .text = "dans_vk tensor network visualiser",
                .color = Color::white,
            }
        );

        const auto& f = live();
        const auto frame_info = std::format(
            "frame {}/{}  {}  ({} tensors, {} bonds, {} abs)",
            current_frame_ + 1,
            frames_.size(),
            f.name.empty() ? "(unnamed)" : f.name,
            f.tensors.size(),
            f.bonds.size(),
            f.abstractions.size()
        );
        frame.draw.text_screen(
            {
                .position = Vec2{24.0f, 56.0f},
                .text = frame_info,
                .color = Color{0.78f, 0.86f, 0.98f, 1.0f},
                .size_scale = 0.62f,
            }
        );

        const auto julia_str = julia_.has_value()
                                   ? (julia_available_ ? "Julia: connected" : "Julia: offline")
                                   : "Julia: disabled";
        const auto info = std::format(
            "selected: {}  hovered: {}  zoom: {:.2f}  world: ({:.0f}, {:.0f})  {}",
            selected_tensors_.size(),
            hovered_tensor_.has_value() ? std::to_string(*hovered_tensor_) : std::string{"-"},
            static_cast<f64>(runtime_->camera_2d_zoom()),
            static_cast<f64>(mouse_world.x),
            static_cast<f64>(mouse_world.y),
            julia_str
        );
        frame.draw.text_screen(
            {
                .position = Vec2{24.0f, 80.0f},
                .text = info,
                .color = Color{0.80f, 0.88f, 0.98f, 1.0f},
                .size_scale = 0.55f,
            }
        );
        frame.draw.text_screen(
            {
                .position = Vec2{24.0f, 104.0f},
                .text = "LMB select/drag  shift+LMB multi  drag leg tip rotates  RMB/MMB pan  wheel zoom",
                .color = Color{0.62f, 0.74f, 0.90f, 1.0f},
                .size_scale = 0.50f,
            }
        );
        frame.draw.text_screen(
            {
                .position = Vec2{24.0f, 124.0f},
                .text = "M/O/P MPS/MPO/PEPS  N tensor  Del delete  +/- prime  C contract  arrows prev/next frame",
                .color = Color{0.62f, 0.74f, 0.90f, 1.0f},
                .size_scale = 0.50f,
            }
        );
        if (not status_message_.empty() and elapsed_ < status_until_)
        {
            frame.draw.text_screen(
                {
                    .position = Vec2{24.0f, 150.0f},
                    .text = status_message_,
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
    std::cout
        << "usage: " << executable
        << " [--load PATH] [--no-julia] [--jl-host HOST] [--jl-port PORT] [--smoke-frames N]"
           " [--screenshot PATH] [--hide-ui]\n";
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
        AppArgs app_args;
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
            else if (arg == "--load" and i + 1 < argc)
            {
                app_args.load_path = argv[++i];
                app_args.seed_demo = false;
            }
            else if (arg == "--no-julia")
            {
                app_args.no_julia = true;
            }
            else if (arg == "--jl-host" and i + 1 < argc)
            {
                app_args.julia.host = argv[++i];
            }
            else if (arg == "--jl-port" and i + 1 < argc)
            {
                app_args.julia.port = static_cast<int>(parse_u32(argv[++i], 8754u));
            }
            else
            {
                std::cerr << "unknown or incomplete argument: " << arg << '\n';
                print_usage(argv[0]);
                return 2;
            }
        }

        Visualizer app{std::move(app_args)};
        dans::vk::Runtime runtime{std::move(config)};
        return runtime.run_prototype(app);
    }
    catch (const std::exception& error)
    {
        std::cerr << "fatal: " << error.what() << '\n';
        return 1;
    }
}
