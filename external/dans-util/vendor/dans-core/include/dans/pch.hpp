// include/dans/pch.hpp

#ifndef DANS_CORE_INCLUDE_DANS_PCH_HPP
#define DANS_CORE_INCLUDE_DANS_PCH_HPP

#pragma once

#include <algorithm>      // IWYU pragma: keep
#include <array>          // IWYU pragma: keep
#include <bit>            // IWYU pragma: keep
#include <cassert>        // IWYU pragma: keep
#include <charconv>       // IWYU pragma: keep
#include <chrono>         // IWYU pragma: keep
#include <cmath>          // IWYU pragma: keep
#include <compare>        // IWYU pragma: keep
#include <concepts>       // IWYU pragma: keep
#include <cstddef>        // IWYU pragma: keep
#include <cstdint>        // IWYU pragma: keep
#include <cstring>        // IWYU pragma: keep
#include <expected>       // IWYU pragma: keep
#include <filesystem>     // IWYU pragma: keep
#include <format>         // IWYU pragma: keep
#include <fstream>        // IWYU pragma: keep
#include <functional>     // IWYU pragma: keep
#include <iostream>       // IWYU pragma: keep
#include <limits>         // IWYU pragma: keep
#include <map>            // IWYU pragma: keep
#include <memory>         // IWYU pragma: keep
#include <numbers>        // IWYU pragma: keep
#include <optional>       // IWYU pragma: keep
#include <print>          // IWYU pragma: keep
#include <ranges>         // IWYU pragma: keep
#include <span>           // IWYU pragma: keep
#include <stdexcept>      // IWYU pragma: keep
#include <string>         // IWYU pragma: keep
#include <string_view>    // IWYU pragma: keep
#include <tuple>          // IWYU pragma: keep
#include <type_traits>    // IWYU pragma: keep
#include <unordered_map>  // IWYU pragma: keep
#include <unordered_set>  // IWYU pragma: keep
#include <utility>        // IWYU pragma: keep
#include <variant>        // IWYU pragma: keep
#include <vector>         // IWYU pragma: keep

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#    include <arm_neon.h>  // IWYU pragma: keep
#endif

#endif  // DANS_CORE_INCLUDE_DANS_PCH_HPP
