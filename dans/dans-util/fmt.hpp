// dans/dans-util/fmt.hpp
// Externals
#include "dans/dans-core/development_markers.hpp"
#include "dans/dans-core/types.hpp"
// StdLib
#include <cstddef>
#include <span>
#include <string>
//

#pragma once
#ifndef DANS_FMT_HPP
#define DANS_FMT_HPP

namespace dans::fmt
{
[[nodiscard]] def format_bytes_binary(u64 bytes) -> std::string;
[[nodiscard]] def format_bytes_decimal(u64 bytes) -> std::string;
[[nodiscard]] def hex_dump(std::span<const std::byte> bytes) -> std::string;
}  // namespace dans::fmt

#endif  // DANS_FMT_HPP
