// dans/vk/config_validity.hpp
//
#pragma once

#include "dans/dans-core/types.hpp"
// StdLib
#include <concepts>
#include <type_traits>
#include <utility>
//

namespace dans::vk
{
// Shared validity contract: every *Validity enum is a u8-backed scoped enum with
// 0 == valid, so is_valid can check any config without naming its enum type.
// validate() is an ADL customization point - define one overload per config
// beside its struct.
inline constexpr u8 k_config_is_valid{0u};

template <typename T>
concept ValidityEnum = std::is_scoped_enum_v<T> and std::same_as<std::underlying_type_t<T>, u8>;

template <typename Config>
concept Validatable = requires(const Config& config) {
    { validate(config) } -> ValidityEnum;
};

template <Validatable Config>
[[nodiscard]] auto is_valid(const Config& config) noexcept -> bool
{
    return std::to_underlying(validate(config)) == k_config_is_valid;
}
}  // namespace dans::vk
