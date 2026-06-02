#pragma once

#include "dans/gfx/color.hpp"
#include "dans/linalg/linalg.hpp"

#include "dans/dans-core/types.hpp"
#include <limits>

namespace dans::vk
{
using dans::linalg::Mat4;
using dans::linalg::Quat;
using dans::linalg::Vec2;
using dans::linalg::Vec3;
using dans::linalg::Vec4;
using dans::linalg::k_axis_x;
using dans::linalg::k_axis_y;
using dans::linalg::k_axis_z;
using dans::linalg::k_quat_identity;

using dans::gfx::Color;
using dans::gfx::ColorU8;
using dans::gfx::color_channel_to_u8;
using dans::gfx::grayscale;
using dans::gfx::luminance;
using dans::gfx::mix_color;
using dans::gfx::to_color;
using dans::gfx::to_color_u8;
using dans::gfx::to_vec4;
using dans::gfx::with_alpha;

inline constexpr auto k_invalid_index = std::numeric_limits<usize>::max();
inline constexpr auto k_invalid_id = std::numeric_limits<u32>::max();

struct MeshHandle
{
    u32 id{k_invalid_id};

    [[nodiscard]] auto valid() const noexcept -> bool
    {
        return id != k_invalid_id;
    }
};
struct TextureHandle
{
    u32 id{k_invalid_id};

    [[nodiscard]] auto valid() const noexcept -> bool
    {
        return id != k_invalid_id;
    }
};
struct ObjectId
{
    u32 value{k_invalid_id};

    [[nodiscard]] auto valid() const noexcept -> bool
    {
        return value != k_invalid_id;
    }
};
}  // namespace dans::vk
