// include/dans/types.hpp

#ifndef DANS_CORE_INCLUDE_DANS_TYPES_HPP
#define DANS_CORE_INCLUDE_DANS_TYPES_HPP
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#if defined(__has_include) and __has_include(<stdfloat>)
#    include <stdfloat>
#endif

namespace dans
{
using usize = std::size_t;
using isize = std::ptrdiff_t;

using uptr = std::uintptr_t;
using iptr = std::intptr_t;

using byte = std::byte;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

#if defined(__STDCPP_FLOAT32_T__) and defined(__STDCPP_FLOAT64_T__)
using f32 = std::float32_t;
using f64 = std::float64_t;
#else
using f32 = float;
using f64 = double;
#endif

static_assert(std::is_same_v<usize, decltype(sizeof(void*))>);
static_assert(
    std::is_same_v<isize, decltype(static_cast<int*>(nullptr) - static_cast<int*>(nullptr))>
);

static_assert(sizeof(byte) == 1zu);

static_assert(sizeof(u8) == 1zu);
static_assert(sizeof(u16) == 2zu);
static_assert(sizeof(u32) == 4zu);
static_assert(sizeof(u64) == 8zu);

static_assert(sizeof(i8) == 1zu);
static_assert(sizeof(i16) == 2zu);
static_assert(sizeof(i32) == 4zu);
static_assert(sizeof(i64) == 8zu);

static_assert(sizeof(f32) == 4zu);
static_assert(sizeof(f64) == 8zu);

}  // namespace dans

#endif  // DANS_CORE_INCLUDE_DANS_TYPES_HPP
