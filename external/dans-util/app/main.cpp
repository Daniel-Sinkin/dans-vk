// app/main.cpp
// Externals
#include <dans/chrono.hpp>
#include <dans/development_markers.hpp>
#include <dans/types.hpp>
// StdLib
#include <chrono>
#include <print>
//

def main() -> int
{
    using Duration = std::chrono::duration<dans::f64>;
    std::println(
        "{}", dans::chrono::format_seconds(Duration{123 * 86400.0 + 13 * 3600 + 22 * 60 + 12})
    );
    std::println("{}", dans::chrono::format_seconds(Duration{5 * 3600.0 + 30}, true));
    std::println("{}", dans::chrono::format_seconds(Duration{0}, true));
}
