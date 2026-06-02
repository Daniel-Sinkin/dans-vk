// include/dans/env.hpp
// Externals
#include <dans/development_markers.hpp>
#include <dans/types.hpp>
// StdLib
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
//

#pragma once
#ifndef DANS_ENV_HPP
#define DANS_ENV_HPP

namespace dans::env
{
[[nodiscard]] def get(std::string_view name) -> std::optional<std::string>;
[[nodiscard]] def get_int(std::string_view name) -> std::optional<i64>;
[[nodiscard]] def get_bool(std::string_view name) -> std::optional<bool>;
[[nodiscard]] def get_path(std::string_view name) -> std::optional<std::filesystem::path>;
}  // namespace dans::env

#endif  // DANS_ENV_HPP
