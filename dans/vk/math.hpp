#pragma once

#include "dans/vk/types.hpp"

namespace dans::vk
{

[[nodiscard]] inline auto normalize_or(Vec3 value, Vec3 fallback) noexcept -> Vec3
{
    const auto length_squared = glm::dot(value, value);
    if (length_squared <= 1.0e-12f)
    {
        return glm::normalize(fallback);
    }
    return value * glm::inversesqrt(length_squared);
}

}  // namespace dans::vk
