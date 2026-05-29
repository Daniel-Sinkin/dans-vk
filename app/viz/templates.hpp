// app/viz/templates.hpp
//
// Scene-level template builders (MPS / MPO / PEPS) ported from the JS
// implementations in tensor-network-visualiser.html lines 1310-1369.
// Each builder takes the current next-id state, mutates it, and returns
// the new tensors + bonds + abstraction.
//
#pragma once

#include "app/viz/types.hpp"
// StdLib
#include <numbers>
#include <vector>
//

namespace viz
{

struct TemplateOutput
{
    std::vector<Tensor> tensors{};
    std::vector<Bond> bonds{};
    Abstraction abstraction{};
};

struct IdCounters
{
    u32 next_id{};
    u32 next_leg_id{};
    u32 next_bond_id{};
    u32 next_abs_id{};
};

namespace detail
{

[[nodiscard]] inline auto make_tensor(
    IdCounters& counters, f32 x, f32 y, std::vector<i32> dims, std::vector<f32> angles
) -> Tensor
{
    Tensor t;
    t.id = counters.next_id++;
    t.label = "T" + std::to_string(t.id);
    t.x = x;
    t.y = y;
    t.rank = static_cast<i32>(dims.size());
    t.leg_id.reserve(dims.size());
    for (usize i = 0; i < dims.size(); ++i)
    {
        t.leg_id.push_back(counters.next_leg_id++);
    }
    t.color = rank_color(t.rank);
    t.leg_prime.assign(dims.size(), 0);
    t.src_inds.resize(dims.size());
    t.dims = std::move(dims);
    if (angles.empty())
    {
        t.leg_angles = default_leg_angles(t.rank);
    }
    else
    {
        t.leg_angles = std::move(angles);
    }
    return t;
}

}  // namespace detail

[[nodiscard]] inline auto build_mps(IdCounters& counters, i32 n, i32 chi, i32 phys, Vec2 center)
    -> TemplateOutput
{
    constexpr auto pi = std::numbers::pi_v<f32>;
    constexpr f32 spacing = 120.0f;
    const auto x0 = center.x - static_cast<f32>(n - 1) * spacing * 0.5f;
    TemplateOutput out;
    out.abstraction.id = counters.next_abs_id++;
    out.abstraction.type_name = "MPS";
    out.abstraction.name = "MPS";
    for (i32 i = 0; i < n; ++i)
    {
        std::vector<i32> dims;
        std::vector<f32> angles;
        if (n == 1)
        {
            dims = {phys};
            angles = {pi * 0.5f};
        }
        else if (i == 0)
        {
            dims = {chi, phys};
            angles = {0.0f, pi * 0.5f};
        }
        else if (i == n - 1)
        {
            dims = {chi, phys};
            angles = {pi, pi * 0.5f};
        }
        else
        {
            dims = {chi, chi, phys};
            angles = {pi, 0.0f, pi * 0.5f};
        }
        auto t = detail::make_tensor(
            counters, x0 + static_cast<f32>(i) * spacing, center.y, std::move(dims), std::move(angles)
        );
        t.abs_id = out.abstraction.id;
        out.abstraction.members.push_back(t.id);
        out.tensors.push_back(std::move(t));
    }
    for (i32 i = 0; i + 1 < n; ++i)
    {
        out.bonds.push_back(
            Bond{
                .id = counters.next_bond_id++,
                .a = out.tensors[static_cast<usize>(i)].id,
                .ai = (i == 0) ? 0 : 1,
                .b = out.tensors[static_cast<usize>(i + 1)].id,
                .bi = 0,
            }
        );
    }
    return out;
}

[[nodiscard]] inline auto build_mpo(IdCounters& counters, i32 n, i32 chi, i32 phys, Vec2 center)
    -> TemplateOutput
{
    constexpr auto pi = std::numbers::pi_v<f32>;
    constexpr f32 spacing = 120.0f;
    const auto x0 = center.x - static_cast<f32>(n - 1) * spacing * 0.5f;
    TemplateOutput out;
    out.abstraction.id = counters.next_abs_id++;
    out.abstraction.type_name = "MPO";
    out.abstraction.name = "MPO";
    for (i32 i = 0; i < n; ++i)
    {
        std::vector<i32> dims;
        std::vector<f32> angles;
        if (n == 1)
        {
            dims = {phys, phys};
            angles = {-pi * 0.5f, pi * 0.5f};
        }
        else if (i == 0)
        {
            dims = {chi, phys, phys};
            angles = {0.0f, -pi * 0.5f, pi * 0.5f};
        }
        else if (i == n - 1)
        {
            dims = {chi, phys, phys};
            angles = {pi, -pi * 0.5f, pi * 0.5f};
        }
        else
        {
            dims = {chi, chi, phys, phys};
            angles = {pi, 0.0f, -pi * 0.5f, pi * 0.5f};
        }
        auto t = detail::make_tensor(
            counters, x0 + static_cast<f32>(i) * spacing, center.y, std::move(dims), std::move(angles)
        );
        t.abs_id = out.abstraction.id;
        out.abstraction.members.push_back(t.id);
        out.tensors.push_back(std::move(t));
    }
    for (i32 i = 0; i + 1 < n; ++i)
    {
        out.bonds.push_back(
            Bond{
                .id = counters.next_bond_id++,
                .a = out.tensors[static_cast<usize>(i)].id,
                .ai = (i == 0) ? 0 : 1,
                .b = out.tensors[static_cast<usize>(i + 1)].id,
                .bi = 0,
            }
        );
    }
    return out;
}

[[nodiscard]] inline auto build_peps(
    IdCounters& counters, i32 rows, i32 cols, i32 chi, i32 phys, Vec2 center
) -> TemplateOutput
{
    constexpr auto pi = std::numbers::pi_v<f32>;
    constexpr f32 spacing = 129.0f;
    const auto x0 = center.x - static_cast<f32>(cols - 1) * spacing * 0.5f;
    const auto y0 = center.y - static_cast<f32>(rows - 1) * spacing * 0.5f;
    TemplateOutput out;
    out.abstraction.id = counters.next_abs_id++;
    out.abstraction.type_name = "PEPS";
    out.abstraction.name = "PEPS";

    struct Cell
    {
        u32 tensor_id{};
        std::vector<std::string> legmap{};
    };
    std::vector<std::vector<Cell>> grid(static_cast<usize>(rows));
    for (auto& row : grid)
    {
        row.resize(static_cast<usize>(cols));
    }

    for (i32 r = 0; r < rows; ++r)
    {
        for (i32 c = 0; c < cols; ++c)
        {
            std::vector<std::string> order;
            if (r > 0)
            {
                order.push_back("up");
            }
            if (r < rows - 1)
            {
                order.push_back("down");
            }
            if (c > 0)
            {
                order.push_back("left");
            }
            if (c < cols - 1)
            {
                order.push_back("right");
            }
            order.push_back("phys");

            std::vector<i32> dims;
            std::vector<f32> angles;
            dims.reserve(order.size());
            angles.reserve(order.size());
            for (const auto& o : order)
            {
                dims.push_back((o == "phys") ? phys : chi);
                if (o == "up")
                {
                    angles.push_back(-pi * 0.5f);
                }
                else if (o == "down")
                {
                    angles.push_back(pi * 0.5f);
                }
                else if (o == "left")
                {
                    angles.push_back(pi);
                }
                else if (o == "right")
                {
                    angles.push_back(0.0f);
                }
                else
                {
                    angles.push_back(-pi * 0.25f);
                }
            }
            auto t = detail::make_tensor(
                counters,
                x0 + static_cast<f32>(c) * spacing,
                y0 + static_cast<f32>(r) * spacing,
                std::move(dims),
                std::move(angles)
            );
            t.abs_id = out.abstraction.id;
            out.abstraction.members.push_back(t.id);
            const auto idx = static_cast<usize>(r);
            const auto jdx = static_cast<usize>(c);
            grid[idx][jdx].tensor_id = t.id;
            grid[idx][jdx].legmap = std::move(order);
            out.tensors.push_back(std::move(t));
        }
    }

    auto find_leg = [](const std::vector<std::string>& legmap, std::string_view name) {
        for (usize i = 0; i < legmap.size(); ++i)
        {
            if (legmap[i] == name)
            {
                return static_cast<i32>(i);
            }
        }
        return -1;
    };

    for (i32 r = 0; r < rows; ++r)
    {
        for (i32 c = 0; c + 1 < cols; ++c)
        {
            const auto idx = static_cast<usize>(r);
            const auto jdx = static_cast<usize>(c);
            out.bonds.push_back(
                Bond{
                    .id = counters.next_bond_id++,
                    .a = grid[idx][jdx].tensor_id,
                    .ai = find_leg(grid[idx][jdx].legmap, "right"),
                    .b = grid[idx][jdx + 1].tensor_id,
                    .bi = find_leg(grid[idx][jdx + 1].legmap, "left"),
                }
            );
        }
    }
    for (i32 r = 0; r + 1 < rows; ++r)
    {
        for (i32 c = 0; c < cols; ++c)
        {
            const auto idx = static_cast<usize>(r);
            const auto jdx = static_cast<usize>(c);
            out.bonds.push_back(
                Bond{
                    .id = counters.next_bond_id++,
                    .a = grid[idx][jdx].tensor_id,
                    .ai = find_leg(grid[idx][jdx].legmap, "down"),
                    .b = grid[idx + 1][jdx].tensor_id,
                    .bi = find_leg(grid[idx + 1][jdx].legmap, "up"),
                }
            );
        }
    }
    return out;
}

}  // namespace viz
