// app/viz/envelope.hpp
//
// JSON envelope I/O for the tensor network visualiser. Matches the
// frontend/backend wire format (variant + version + content) used by
// the existing tensor-network-visualiser.html and server.jl.
//
#pragma once

#include "app/viz/types.hpp"
// Externals
#include <nlohmann/json.hpp>
// StdLib
#include <algorithm>
#include <numbers>
#include <stdexcept>
#include <unordered_map>
//

namespace viz
{

using json = nlohmann::json;

inline constexpr i32 k_export_version = 1;

// ---------------------------------------------------------------------------
// Tensor content envelope
// ---------------------------------------------------------------------------

[[nodiscard]] inline auto tensor_content_envelope(const Tensor& t) -> json
{
    json indices = json::array();
    for (i32 i = 0; i < t.rank; ++i)
    {
        const auto idx = static_cast<usize>(i);
        const auto* src = (idx < t.src_inds.size()) ? &t.src_inds[idx] : nullptr;
        const auto dim = (idx < t.dims.size()) ? t.dims[idx] : 1;
        const auto plev = (idx < t.leg_prime.size()) ? t.leg_prime[idx] : 0;
        json entry;
        entry["id"] = (src != nullptr and not src->id.empty())
                          ? src->id
                          : (std::string{"synth_"} + std::to_string(t.id) + "_" + std::to_string(i));
        entry["dim"] = dim;
        entry["tags"] = (src != nullptr) ? json(src->tags) : json::array();
        entry["plev"] = (src != nullptr) ? src->plev : plev;
        indices.push_back(std::move(entry));
    }
    const auto is_complex = t.data_im.has_value();
    json data = json::array();
    if (is_complex)
    {
        const auto& im = *t.data_im;
        const auto n = t.data.size();
        for (usize i = 0; i < n; ++i)
        {
            data.push_back(t.data[i]);
            data.push_back((i < im.size()) ? im[i] : 0.0);
        }
    }
    else
    {
        for (const auto v : t.data)
        {
            data.push_back(v);
        }
    }
    json content;
    content["eltype"] = is_complex ? "ComplexF64" : "Float64";
    content["indices"] = std::move(indices);
    content["data_order"] = "col_major";
    content["data"] = std::move(data);
    return content;
}

[[nodiscard]] inline auto tensor_envelope(const Tensor& t) -> json
{
    json env;
    env["version"] = k_export_version;
    env["variant"] = "Tensor";
    env["content"] = tensor_content_envelope(t);
    return env;
}

[[nodiscard]] inline auto container_to_chain_envelope(
    const Abstraction& abs, const std::vector<Tensor>& tensors
) -> json
{
    const auto variant = (abs.type_name == "MPS" or abs.type_name == "Environment")
                             ? std::string{"MPS"}
                             : std::string{"MPO"};

    json tensors_json = json::array();
    for (const auto member_id : abs.members)
    {
        const Tensor* found = nullptr;
        for (const auto& t : tensors)
        {
            if (t.id == member_id)
            {
                found = &t;
                break;
            }
        }
        if (found == nullptr)
        {
            throw std::runtime_error("container_to_chain_envelope: member id not found");
        }
        tensors_json.push_back(tensor_envelope(*found));
    }
    json content;
    content["length"] = abs.members.size();
    content["llim"] = 0;
    content["rlim"] = abs.members.size();
    content["tensors"] = std::move(tensors_json);

    json env;
    env["version"] = k_export_version;
    env["variant"] = variant;
    env["content"] = std::move(content);
    return env;
}

// ---------------------------------------------------------------------------
// Tensor parse
// ---------------------------------------------------------------------------

struct TensorBuildContext
{
    u32 id{};
    std::string label{};
    f32 x{};
    f32 y{};
    std::vector<u32> leg_id{};
    std::optional<std::vector<f32>> leg_angles{};
};

[[nodiscard]] inline auto tensor_from_content(const json& content, TensorBuildContext ctx)
    -> Tensor
{
    Tensor t;
    t.id = ctx.id;
    t.label = std::move(ctx.label);
    t.x = ctx.x;
    t.y = ctx.y;
    t.leg_id = std::move(ctx.leg_id);

    const auto& indices = content.at("indices");
    t.rank = static_cast<i32>(indices.size());
    t.dims.reserve(static_cast<usize>(t.rank));
    t.leg_prime.reserve(static_cast<usize>(t.rank));
    t.src_inds.reserve(static_cast<usize>(t.rank));
    for (const auto& ind : indices)
    {
        t.dims.push_back(static_cast<i32>(ind.value("dim", 1)));
        t.leg_prime.push_back(static_cast<i32>(ind.value("plev", 0)));
        SrcIndex sx;
        if (ind.contains("id"))
        {
            sx.id = ind["id"].is_string() ? ind["id"].get<std::string>()
                                          : std::to_string(ind["id"].get<i64>());
        }
        sx.plev = static_cast<i32>(ind.value("plev", 0));
        if (ind.contains("tags") and ind["tags"].is_array())
        {
            for (const auto& tag : ind["tags"])
            {
                sx.tags.push_back(tag.get<std::string>());
            }
        }
        t.src_inds.push_back(std::move(sx));
    }
    t.color = rank_color(t.rank);
    if (ctx.leg_angles.has_value())
    {
        t.leg_angles = std::move(*ctx.leg_angles);
    }
    else
    {
        t.leg_angles = default_leg_angles(t.rank);
    }

    const auto eltype = content.value("eltype", std::string{"Float64"});
    const auto is_complex = eltype.rfind("Complex", 0) == 0;

    usize total = 1;
    for (const auto d : t.dims)
    {
        total *= static_cast<usize>(std::max(1, d));
    }
    t.data.assign(total, 0.0);
    if (content.contains("data") and content["data"].is_array())
    {
        const auto& raw = content["data"];
        if (is_complex)
        {
            std::vector<f64> im(total, 0.0);
            for (usize i = 0; i < total; ++i)
            {
                if (2u * i + 1u < raw.size())
                {
                    t.data[i] = raw[2 * i].get<f64>();
                    im[i] = raw[2 * i + 1].get<f64>();
                }
            }
            t.data_im = std::move(im);
        }
        else
        {
            for (usize i = 0; i < total and i < raw.size(); ++i)
            {
                t.data[i] = raw[i].get<f64>();
            }
        }
    }
    return t;
}

// ---------------------------------------------------------------------------
// Bond detection from indices (chain/grid frames)
// ---------------------------------------------------------------------------

struct DetectedBondsResult
{
    std::vector<Bond> bonds{};
    u32 next_bond_id{};
};

[[nodiscard]] inline auto detect_bonds_from_indices(
    const std::vector<Tensor>& tensors, u32 start_bond_id
) -> DetectedBondsResult
{
    std::unordered_map<std::string, std::vector<std::pair<usize, usize>>> by_key;
    for (usize ti = 0; ti < tensors.size(); ++ti)
    {
        const auto& t = tensors[ti];
        for (usize li = 0; li < t.src_inds.size(); ++li)
        {
            const auto& s = t.src_inds[li];
            const auto key = s.id + "|" + std::to_string(s.plev);
            by_key[key].push_back({ti, li});
        }
    }
    DetectedBondsResult result;
    result.next_bond_id = start_bond_id;
    for (const auto& [_, pairs] : by_key)
    {
        if (pairs.size() == 2)
        {
            const auto& [ati, ai] = pairs[0];
            const auto& [bti, bi] = pairs[1];
            result.bonds.push_back(
                Bond{
                    .id = result.next_bond_id++,
                    .a = tensors[ati].id,
                    .ai = static_cast<i32>(ai),
                    .b = tensors[bti].id,
                    .bi = static_cast<i32>(bi),
                }
            );
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Layout angle assignment (matches assignAnglesByLayout)
// ---------------------------------------------------------------------------

[[nodiscard]] inline auto tag_driven_angle(std::span<const std::string> tags, i32 plev) noexcept
    -> std::optional<f32>
{
    constexpr auto pi = std::numbers::pi_v<f32>;
    for (const auto& tag : tags)
    {
        if (tag == "phys")
        {
            return (plev > 0) ? -3.0f * pi / 4.0f : pi / 4.0f;
        }
        if (tag == "hlink")
        {
            return 0.0f;
        }
        if (tag == "vlink")
        {
            return pi / 2.0f;
        }
    }
    return std::nullopt;
}

inline auto assign_angles_by_layout(
    std::vector<Tensor>& tensors, const std::vector<Bond>& bonds, std::string_view layout
) -> void
{
    // Map tensor id -> {used_leg_indices}
    std::unordered_map<u32, std::vector<u8>> used;
    for (auto& t : tensors)
    {
        used.emplace(t.id, std::vector<u8>(static_cast<usize>(std::max(0, t.rank)), 0u));
    }
    for (const auto& b : bonds)
    {
        auto& av = used[b.a];
        if (b.ai >= 0 and static_cast<usize>(b.ai) < av.size())
        {
            av[static_cast<usize>(b.ai)] = 1u;
        }
        auto& bv = used[b.b];
        if (b.bi >= 0 and static_cast<usize>(b.bi) < bv.size())
        {
            bv[static_cast<usize>(b.bi)] = 1u;
        }
    }

    constexpr auto pi = std::numbers::pi_v<f32>;

    for (auto& t : tensors)
    {
        const auto& mark = used[t.id];
        std::vector<i32> free;
        for (i32 i = 0; i < t.rank; ++i)
        {
            if (static_cast<usize>(i) >= mark.size() or mark[static_cast<usize>(i)] == 0u)
            {
                free.push_back(i);
            }
        }
        if (free.empty())
        {
            continue;
        }
        std::vector<f32> bases(free.size(), 0.0f);
        if (layout == "MPS")
        {
            const auto n = static_cast<f32>(free.size());
            const auto range = std::min(pi * 0.55f, 0.5f + n * 0.18f);
            if (n == 1.0f)
            {
                bases[0] = pi / 2.0f;
            }
            else
            {
                for (usize k = 0; k < free.size(); ++k)
                {
                    bases[k] = pi / 2.0f - range * 0.5f + range * static_cast<f32>(k) / (n - 1.0f);
                }
            }
        }
        else if (layout == "MPO")
        {
            for (usize k = 0; k < free.size(); ++k)
            {
                const auto side = ((k & 1u) == 0u) ? pi / 2.0f : -pi / 2.0f;
                const auto tier = static_cast<f32>(k >> 1u);
                const auto off = tier * 0.45f;
                const auto signed_off = (static_cast<usize>(tier) % 2u == 0u) ? off : -off;
                bases[k] = side + signed_off;
            }
        }
        else if (layout == "PEPS")
        {
            const auto n = static_cast<f32>(free.size());
            const auto center = -pi / 4.0f;
            const auto range = std::min(pi * 0.45f, 0.4f + n * 0.15f);
            if (n == 1.0f)
            {
                bases[0] = center;
            }
            else
            {
                for (usize k = 0; k < free.size(); ++k)
                {
                    bases[k] = center - range * 0.5f + range * static_cast<f32>(k) / (n - 1.0f);
                }
            }
        }
        else
        {
            const auto defs = default_leg_angles(t.rank);
            for (usize k = 0; k < free.size(); ++k)
            {
                bases[k] = (static_cast<usize>(free[k]) < defs.size())
                               ? defs[static_cast<usize>(free[k])]
                               : 0.0f;
            }
        }

        if (t.leg_angles.size() < static_cast<usize>(t.rank))
        {
            t.leg_angles.resize(static_cast<usize>(t.rank), 0.0f);
        }
        for (usize k = 0; k < free.size(); ++k)
        {
            const auto li = static_cast<usize>(free[k]);
            const auto* sx = (li < t.src_inds.size()) ? &t.src_inds[li] : nullptr;
            std::optional<f32> tag_angle;
            if (sx != nullptr)
            {
                tag_angle = tag_driven_angle(sx->tags, sx->plev);
            }
            t.leg_angles[li] = tag_angle.has_value() ? *tag_angle : bases[k];
        }
    }
}

// ---------------------------------------------------------------------------
// Frame finalize
// ---------------------------------------------------------------------------

inline auto frame_finalize(
    Frame& f,
    std::vector<Tensor> tensors,
    std::vector<Bond> bonds,
    std::vector<Abstraction> abstractions
) -> void
{
    u32 max_tid = 0;
    u32 max_leg = 0;
    for (const auto& t : tensors)
    {
        max_tid = std::max(max_tid, t.id);
        for (const auto l : t.leg_id)
        {
            max_leg = std::max(max_leg, l);
        }
    }
    u32 max_bond = 0;
    for (const auto& b : bonds)
    {
        max_bond = std::max(max_bond, b.id);
    }
    u32 max_abs = 0;
    for (const auto& a : abstractions)
    {
        max_abs = std::max(max_abs, a.id);
    }
    f.tensors = std::move(tensors);
    f.bonds = std::move(bonds);
    f.abstractions = std::move(abstractions);
    f.next_id = max_tid + 1;
    f.next_label = max_tid + 1;
    f.next_bond_id = max_bond + 1;
    f.next_leg_id = max_leg + 1;
    f.next_abs_id = max_abs + 1;
}

// ---------------------------------------------------------------------------
// Frame builders per envelope variant
// ---------------------------------------------------------------------------

inline auto frame_from_tensor_env(
    Frame& f, const json& env, std::string name, Vec2 stage_center
) -> void
{
    f.name = std::move(name);
    const auto& content = env.at("content");
    const auto indices_count = static_cast<usize>(content.at("indices").size());
    std::vector<u32> leg_id;
    leg_id.reserve(indices_count);
    for (usize k = 0; k < indices_count; ++k)
    {
        leg_id.push_back(static_cast<u32>(k + 1));
    }
    auto t = tensor_from_content(
        content,
        TensorBuildContext{
            .id = 1,
            .label = "T1",
            .x = stage_center.x,
            .y = stage_center.y,
            .leg_id = std::move(leg_id),
        }
    );
    frame_finalize(f, {std::move(t)}, {}, {});
}

inline auto frame_from_chain_env(
    Frame& f,
    const json& env,
    std::string name,
    std::string_view layout_kind,
    Vec2 stage_center
) -> void
{
    f.name = std::move(name);
    const auto& content = env.at("content");
    const auto n = static_cast<i32>(content.value("length", 0));
    constexpr f32 spacing = 130.0f;
    const auto x0 = stage_center.x - static_cast<f32>(n - 1) * spacing * 0.5f;
    const auto cy = stage_center.y;
    std::vector<Tensor> tensor_list;
    tensor_list.reserve(static_cast<usize>(std::max(0, n)));
    u32 leg_counter = 1;
    for (i32 i = 0; i < n; ++i)
    {
        const auto& t_env = content.at("tensors").at(static_cast<usize>(i));
        const auto& t_content = t_env.at("content");
        const auto leg_count = static_cast<usize>(t_content.at("indices").size());
        std::vector<u32> leg_id;
        leg_id.reserve(leg_count);
        for (usize k = 0; k < leg_count; ++k)
        {
            leg_id.push_back(leg_counter++);
        }
        tensor_list.push_back(
            tensor_from_content(
                t_content,
                TensorBuildContext{
                    .id = static_cast<u32>(i + 1),
                    .label = std::string{"T"} + std::to_string(i + 1),
                    .x = x0 + static_cast<f32>(i) * spacing,
                    .y = cy,
                    .leg_id = std::move(leg_id),
                }
            )
        );
    }
    auto detect = detect_bonds_from_indices(tensor_list, 1);
    assign_angles_by_layout(tensor_list, detect.bonds, layout_kind);
    constexpr u32 abs_id = 1;
    std::vector<Abstraction> abstractions;
    Abstraction abs;
    abs.id = abs_id;
    abs.type_name = std::string{layout_kind};
    abs.name = std::string{layout_kind};
    for (const auto& t : tensor_list)
    {
        abs.members.push_back(t.id);
    }
    for (auto& t : tensor_list)
    {
        t.abs_id = abs_id;
    }
    abstractions.push_back(std::move(abs));
    frame_finalize(f, std::move(tensor_list), std::move(detect.bonds), std::move(abstractions));
    f.next_bond_id = detect.next_bond_id;
}

inline auto frame_from_peps_env(
    Frame& f, const json& env, std::string name, Vec2 stage_center
) -> void
{
    f.name = std::move(name);
    const auto& content = env.at("content");
    const auto nr = static_cast<i32>(content.value("num_rows", 0));
    const auto nc = static_cast<i32>(content.value("num_cols", 0));
    constexpr f32 spacing = 130.0f;
    const auto x0 = stage_center.x - static_cast<f32>(nc - 1) * spacing * 0.5f;
    const auto y0 = stage_center.y - static_cast<f32>(nr - 1) * spacing * 0.5f;
    std::vector<Tensor> tensor_list;
    u32 leg_counter = 1;
    u32 next_tid = 1;
    for (i32 r = 0; r < nr; ++r)
    {
        for (i32 col = 0; col < nc; ++col)
        {
            const auto& t_env = content.at("tensors").at(static_cast<usize>(r)).at(static_cast<usize>(col));
            const auto& t_content = t_env.at("content");
            const auto leg_count = static_cast<usize>(t_content.at("indices").size());
            std::vector<u32> leg_id;
            leg_id.reserve(leg_count);
            for (usize k = 0; k < leg_count; ++k)
            {
                leg_id.push_back(leg_counter++);
            }
            tensor_list.push_back(
                tensor_from_content(
                    t_content,
                    TensorBuildContext{
                        .id = next_tid,
                        .label = std::string{"T"} + std::to_string(next_tid),
                        .x = x0 + static_cast<f32>(col) * spacing,
                        .y = y0 + static_cast<f32>(r) * spacing,
                        .leg_id = std::move(leg_id),
                    }
                )
            );
            ++next_tid;
        }
    }
    auto detect = detect_bonds_from_indices(tensor_list, 1);
    assign_angles_by_layout(tensor_list, detect.bonds, "PEPS");
    constexpr u32 abs_id = 1;
    Abstraction abs;
    abs.id = abs_id;
    abs.type_name = "PEPS";
    abs.name = "PEPS";
    for (const auto& t : tensor_list)
    {
        abs.members.push_back(t.id);
    }
    for (auto& t : tensor_list)
    {
        t.abs_id = abs_id;
    }
    std::vector<Abstraction> abstractions{std::move(abs)};
    frame_finalize(f, std::move(tensor_list), std::move(detect.bonds), std::move(abstractions));
    f.next_bond_id = detect.next_bond_id;
}

// Forward declaration so build_frame can dispatch to Environment/Snapshot,
// which both delegate back to build_frame recursively.
inline auto build_frame(const json& env, std::string name, Vec2 stage_center) -> Frame;

inline auto frame_from_environment_env(
    Frame& f, const json& env, std::string name, Vec2 stage_center
) -> void
{
    f.name = std::move(name);
    if (not env.contains("content") or not env["content"].is_object())
    {
        return;
    }
    const auto& content = env["content"];
    if (not content.contains("mps"))
    {
        return;
    }
    auto inner = build_frame(content["mps"], f.name, stage_center);
    if (not inner.abstractions.empty())
    {
        inner.abstractions[0].type_name = "Environment";
        std::string suffix;
        if (content.contains("layer") and content["layer"].is_string())
        {
            const auto layer = content["layer"].get<std::string>();
            if (not layer.empty())
            {
                suffix += "/" + layer;
            }
        }
        if (content.contains("lognorm") and content["lognorm"].is_number())
        {
            const auto ln = content["lognorm"].get<f64>();
            char buf[32];
            std::snprintf(buf, sizeof(buf), " ln=%.3f", ln);
            suffix += buf;
        }
        inner.abstractions[0].env_suffix = std::move(suffix);
    }
    f.tensors = std::move(inner.tensors);
    f.bonds = std::move(inner.bonds);
    f.abstractions = std::move(inner.abstractions);
    f.next_id = inner.next_id;
    f.next_label = inner.next_label;
    f.next_bond_id = inner.next_bond_id;
    f.next_leg_id = inner.next_leg_id;
    f.next_abs_id = inner.next_abs_id;
}

inline auto frame_from_snapshot_env(
    Frame& f, const json& env, std::string name, Vec2 stage_center
) -> void
{
    const auto& content = env.value("content", json::object());
    const auto items = content.value("items", json::array());
    const auto label
        = content.contains("label") and content["label"].is_string()
              ? content["label"].get<std::string>()
              : std::string{};
    f.label = label;
    if (content.contains("description") and content["description"].is_string())
    {
        f.description = content["description"].get<std::string>();
    }
    f.name = name + (label.empty() ? std::string{} : (" [" + label + "]"));

    if (items.empty())
    {
        return;
    }
    struct Sub
    {
        std::string name;
        Frame frame;
    };
    std::vector<Sub> subs;
    subs.reserve(items.size());
    for (const auto& item : items)
    {
        Sub s;
        s.name = item.value("name", std::string{});
        s.frame = build_frame(item.at("payload"), name + ":" + s.name, stage_center);
        subs.push_back(std::move(s));
    }
    auto bbox_of = [](const Frame& frame) {
        f32 min_x = std::numeric_limits<f32>::infinity();
        f32 max_x = -std::numeric_limits<f32>::infinity();
        f32 min_y = min_x;
        f32 max_y = max_x;
        for (const auto& t : frame.tensors)
        {
            min_x = std::min(min_x, t.x);
            max_x = std::max(max_x, t.x);
            min_y = std::min(min_y, t.y);
            max_y = std::max(max_y, t.y);
        }
        if (frame.tensors.empty())
        {
            min_x = max_x = min_y = max_y = 0.0f;
        }
        return std::array<f32, 4>{min_x, max_x, min_y, max_y};
    };
    constexpr f32 gap = 220.0f;
    f32 total_w = 0.0f;
    std::vector<std::array<f32, 4>> boxes;
    boxes.reserve(subs.size());
    for (const auto& s : subs)
    {
        const auto box = bbox_of(s.frame);
        boxes.push_back(box);
        total_w += std::max(80.0f, box[1] - box[0]);
    }
    if (not subs.empty())
    {
        total_w += gap * static_cast<f32>(subs.size() - 1);
    }

    auto left_x = stage_center.x - total_w * 0.5f;
    std::vector<Tensor> all_t;
    std::vector<Bond> all_b;
    std::vector<Abstraction> all_a;
    u32 tid_off = 0;
    u32 leg_off = 0;
    u32 bid_off = 0;
    u32 aid_off = 0;

    for (usize k = 0; k < subs.size(); ++k)
    {
        auto& sub = subs[k];
        const auto& box = boxes[k];
        const auto sub_w = std::max(80.0f, box[1] - box[0]);
        const auto sub_cx = left_x + sub_w * 0.5f;
        left_x += sub_w + gap;
        const auto dx = sub_cx - 0.5f * (box[0] + box[1]);
        const auto dy = stage_center.y - 0.5f * (box[2] + box[3]);
        for (auto& t : sub.frame.tensors)
        {
            t.id += tid_off;
            t.x += dx;
            t.y += dy;
            for (auto& l : t.leg_id)
            {
                l += leg_off;
            }
            if (t.abs_id.has_value())
            {
                t.abs_id = *t.abs_id + aid_off;
            }
            all_t.push_back(std::move(t));
        }
        for (auto& b : sub.frame.bonds)
        {
            b.id += bid_off;
            b.a += tid_off;
            b.b += tid_off;
            all_b.push_back(std::move(b));
        }
        for (auto& ab : sub.frame.abstractions)
        {
            ab.id += aid_off;
            const auto suffix = ab.env_suffix;
            ab.name = sub.name + " (" + ab.type_name + suffix + ")";
            for (auto& m : ab.members)
            {
                m += tid_off;
            }
            all_a.push_back(std::move(ab));
        }
        tid_off += sub.frame.next_id;
        leg_off += sub.frame.next_leg_id;
        bid_off += sub.frame.next_bond_id;
        aid_off += sub.frame.next_abs_id;
    }
    frame_finalize(f, std::move(all_t), std::move(all_b), std::move(all_a));
}

[[nodiscard]] inline auto build_frame(const json& env, std::string name, Vec2 stage_center)
    -> Frame
{
    Frame f;
    f.name = name;
    if (not env.contains("version") or static_cast<i32>(env["version"].get<i64>()) != k_export_version)
    {
        throw std::runtime_error("unsupported envelope version");
    }
    const auto variant = env.value("variant", std::string{});
    if (variant == "Tensor")
    {
        frame_from_tensor_env(f, env, std::move(name), stage_center);
    }
    else if (variant == "MPS")
    {
        frame_from_chain_env(f, env, std::move(name), "MPS", stage_center);
    }
    else if (variant == "MPO")
    {
        frame_from_chain_env(f, env, std::move(name), "MPO", stage_center);
    }
    else if (variant == "PEPS" or variant == "TensorGrid")
    {
        frame_from_peps_env(f, env, std::move(name), stage_center);
    }
    else if (variant == "Environment")
    {
        frame_from_environment_env(f, env, std::move(name), stage_center);
    }
    else if (variant == "Snapshot")
    {
        frame_from_snapshot_env(f, env, std::move(name), stage_center);
    }
    else
    {
        throw std::runtime_error("unknown envelope variant: " + variant);
    }
    return f;
}

}  // namespace viz
