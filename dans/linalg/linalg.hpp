// dans/linalg/linalg.hpp
//
#pragma once

#include "dans/dans-core/types.hpp"
// Externals
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
//

namespace dans::linalg
{
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Quat = glm::quat;
using Mat4 = glm::mat4;

inline constexpr Vec3 k_axis_x{1.0f, 0.0f, 0.0f};
inline constexpr Vec3 k_axis_y{0.0f, 1.0f, 0.0f};
inline constexpr Vec3 k_axis_z{0.0f, 0.0f, 1.0f};
inline constexpr Quat k_quat_identity{1.0f, 0.0f, 0.0f, 0.0f};
// glm's scalar matrix ctor fills the diagonal: this is the identity, not all-ones.
inline constexpr Mat4 k_mat4_identity{1.0f};

[[nodiscard]] inline auto normalize_or(Vec3 value, Vec3 fallback) noexcept -> Vec3
{
    const auto length_squared = glm::dot(value, value);
    if (length_squared <= 1.0e-12f)
    {
        return glm::normalize(fallback);
    }
    return value * glm::inversesqrt(length_squared);
}
}  // namespace dans::linalg
