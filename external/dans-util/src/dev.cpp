// src/dev.cpp
//
#include <dans/dev.hpp>
// StdLib
#include <cstdio>
#include <cstdlib>
#include <print>
#include <string_view>
//

namespace dans::dev
{
[[noreturn]] def panic_impl(std::string_view msg, std::string_view file, int line) -> void
{
    std::println(stderr, "PANIC at {}:{}: {}", file, line, msg);
    std::abort();
}
}  // namespace dans::dev
