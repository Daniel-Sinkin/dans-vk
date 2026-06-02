// include/dans/lit.hpp
// Externals
#include <dans/development_markers.hpp>
#include <dans/types.hpp>
//

#pragma once
#ifndef DANS_LIT_HPP
#define DANS_LIT_HPP

namespace dans::lit
{
[[nodiscard]] consteval def operator""_byte(unsigned long long n) -> u64
{
    return n;
}

[[nodiscard]] consteval def operator""_kib(unsigned long long n) -> u64
{
    return n * 1024ULL;
}

[[nodiscard]] consteval def operator""_mib(unsigned long long n) -> u64
{
    return n * 1024ULL * 1024ULL;
}

[[nodiscard]] consteval def operator""_gib(unsigned long long n) -> u64
{
    return n * 1024ULL * 1024ULL * 1024ULL;
}

[[nodiscard]] consteval def operator""_tib(unsigned long long n) -> u64
{
    return n * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
}

[[nodiscard]] consteval def operator""_kb(unsigned long long n) -> u64
{
    return n * 1000ULL;
}

[[nodiscard]] consteval def operator""_mb(unsigned long long n) -> u64
{
    return n * 1000ULL * 1000ULL;
}

[[nodiscard]] consteval def operator""_gb(unsigned long long n) -> u64
{
    return n * 1000ULL * 1000ULL * 1000ULL;
}

[[nodiscard]] consteval def operator""_tb(unsigned long long n) -> u64
{
    return n * 1000ULL * 1000ULL * 1000ULL * 1000ULL;
}
}  // namespace dans::lit

#endif  // DANS_LIT_HPP
