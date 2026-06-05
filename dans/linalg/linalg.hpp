// dans/linalg/linalg.hpp
//
#pragma once

#include "dans/dans-core/types.hpp"
// Externals
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
// StdLib
#include <type_traits>
//

namespace dans::linalg
{
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Quat = glm::quat;
using Mat4 = glm::mat4;

// glm's index type (operator[], .length()) is forced to usize via
// GLM_FORCE_SIZE_T_LENGTH (set globally in CMake) so indexing lines up with
// usize. Fails the build if that definition ever goes missing.
static_assert(
    std::is_same_v<glm::length_t, usize>,
    "GLM_FORCE_SIZE_T_LENGTH must be defined globally so glm's index type is usize"
);

inline constexpr Vec3 k_axis_x{1.0f, 0.0f, 0.0f};
inline constexpr Vec3 k_axis_y{0.0f, 1.0f, 0.0f};
inline constexpr Vec3 k_axis_z{0.0f, 0.0f, 1.0f};
inline constexpr Quat k_quat_identity{1.0f, 0.0f, 0.0f, 0.0f};
inline constexpr Mat4 k_mat4_identity{1.0f};

[[nodiscard]] inline auto normalize_or(Vec3 value, Vec3 fallback, f32 eps = 1.0e-12f) noexcept
    -> Vec3
{
    const auto length_squared = glm::dot(value, value);
    if (length_squared <= eps)
    {
        return glm::normalize(fallback);
    }
    return value * glm::inversesqrt(length_squared);
}
}  // namespace dans::linalg
