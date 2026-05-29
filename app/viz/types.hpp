// app/viz/types.hpp
//
// Tensor network visualiser data model. Ported from the JavaScript
// object structure in tensor-network-visualiser.html so JSON envelopes
// round-trip directly between this app, .jsonl files, and the Julia
// /contract server.
//
#pragma once

#include "dans/vk/types.hpp"
// StdLib
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
//

namespace viz
{

using dans::vk::Color;
using dans::vk::Vec2;
using dans::vk::f32;
using dans::vk::f64;
using dans::vk::i32;
using dans::vk::i64;
using dans::vk::u32;
using dans::vk::u8;
using dans::vk::usize;

// Per-leg metadata that round-trips through the JSON envelope.
// 'id' is the index identifier produced by the simulator; bonds are
// detected by matching (id, plev) pairs across tensors.
struct SrcIndex
{
    std::string id{};
    i32 plev{};
    std::vector<std::string> tags{};
};

struct Tensor
{
    u32 id{};
    std::string label{};
    f32 x{};
    f32 y{};
    i32 rank{};
    std::vector<i32> dims{};
    std::vector<f32> leg_angles{};
    std::vector<i32> leg_prime{};
    std::vector<u32> leg_id{};
    Color color{Color::white};
    std::vector<f64> data{};
    std::optional<std::vector<f64>> data_im{};
    std::vector<SrcIndex> src_inds{};
    std::optional<u32> abs_id{};
};

struct Bond
{
    u32 id{};
    u32 a{};
    i32 ai{};
    u32 b{};
    i32 bi{};
};

struct Abstraction
{
    u32 id{};
    std::string type_name{};  // "MPS" | "MPO" | "PEPS" | "Environment" | "Snapshot"
    std::string name{};
    std::vector<u32> members{};
    std::string env_suffix{};  // populated by Environment frames
};

struct Frame
{
    std::string name{};
    std::string source{};
    std::string label{};
    std::string description{};
    std::vector<Tensor> tensors{};
    std::vector<Bond> bonds{};
    std::vector<Abstraction> abstractions{};
    u32 next_id{1};
    u32 next_label{1};
    u32 next_bond_id{1};
    u32 next_leg_id{1};
    u32 next_abs_id{1};
};

// HSL palette for tensor nodes by rank, matching RANK_PALETTE in the HTML.
inline constexpr std::array<std::array<f32, 3>, 7> k_rank_palette{{
    {210.0f, 18.0f, 30.0f},
    {200.0f, 60.0f, 60.0f},
    {28.0f, 80.0f, 56.0f},
    {138.0f, 50.0f, 58.0f},
    {300.0f, 50.0f, 60.0f},
    {48.0f, 80.0f, 60.0f},
    {28.0f, 50.0f, 50.0f},
}};

[[nodiscard]] inline auto hsl_to_rgb(f32 h, f32 s, f32 l) noexcept -> Color
{
    h = std::fmod(h, 360.0f);
    if (h < 0.0f)
    {
        h += 360.0f;
    }
    const auto sn = std::clamp(s / 100.0f, 0.0f, 1.0f);
    const auto ln = std::clamp(l / 100.0f, 0.0f, 1.0f);
    const auto c = (1.0f - std::fabs(2.0f * ln - 1.0f)) * sn;
    const auto hp = h / 60.0f;
    const auto x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    f32 r{};
    f32 g{};
    f32 b{};
    if (hp < 1.0f)
    {
        r = c;
        g = x;
    }
    else if (hp < 2.0f)
    {
        r = x;
        g = c;
    }
    else if (hp < 3.0f)
    {
        g = c;
        b = x;
    }
    else if (hp < 4.0f)
    {
        g = x;
        b = c;
    }
    else if (hp < 5.0f)
    {
        r = x;
        b = c;
    }
    else
    {
        r = c;
        b = x;
    }
    const auto m = ln - 0.5f * c;
    return Color{r + m, g + m, b + m, 1.0f};
}

[[nodiscard]] inline auto rank_hsl(i32 rank) noexcept -> std::array<f32, 3>
{
    const auto r = std::clamp(rank, 0, std::numeric_limits<i32>::max());
    if (static_cast<usize>(r) < k_rank_palette.size())
    {
        return k_rank_palette[static_cast<usize>(r)];
    }
    return {std::fmod(static_cast<f32>(r) * 53.0f, 360.0f), 52.0f, 58.0f};
}

[[nodiscard]] inline auto rank_color(i32 rank) noexcept -> Color
{
    const auto hsl = rank_hsl(rank);
    return hsl_to_rgb(hsl[0], hsl[1], hsl[2]);
}

[[nodiscard]] inline auto default_leg_angles(i32 rank) -> std::vector<f32>
{
    std::vector<f32> out;
    out.reserve(static_cast<usize>(std::max(0, rank)));
    constexpr auto two_pi = 2.0f * 3.14159265358979323846f;
    for (i32 i = 0; i < rank; ++i)
    {
        out.push_back(-0.5f * 3.14159265358979323846f + static_cast<f32>(i) * two_pi / static_cast<f32>(rank));
    }
    return out;
}

[[nodiscard]] inline auto tag_color(std::string_view tag) noexcept -> Color
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

[[nodiscard]] inline auto first_tag_color(std::span<const std::string> tags) noexcept -> Color
{
    if (tags.empty())
    {
        return tag_color("");
    }
    return tag_color(tags.front());
}

}  // namespace viz
