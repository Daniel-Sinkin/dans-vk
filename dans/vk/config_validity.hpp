// dans/vk/config_validity.hpp
//
#pragma once

#include "dans/dans-core/types.hpp"
//

namespace dans::vk
{
// Shared validity contract: every *Validity enum is u8 with 0 == valid, so
// is_valid can check any config without naming its enum type. validate() is an
// ADL customization point - define one overload per config beside its struct.
inline constexpr u8 k_config_is_valid{0u};

template <typename Config>
[[nodiscard]] auto is_valid(const Config& config) noexcept -> bool
{
    return static_cast<u8>(validate(config)) == k_config_is_valid;
}
}  // namespace dans::vk
