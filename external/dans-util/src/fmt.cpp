// src/fmt.cpp
//
#include <dans/fmt.hpp>
// Externals
#include <dans/development_markers.hpp>
#include <dans/types.hpp>
// StdLib
#include <array>
#include <cstddef>
#include <format>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
//

namespace dans::fmt
{
namespace
{
[[nodiscard]] def format_bytes_with(
    u64 bytes, f64 divisor, const std::array<std::string_view, 7>& suffixes) -> std::string
{
    mut auto value = static_cast<f64>(bytes);
    mut auto index = 0zu;
    while (value >= divisor and index + 1 < suffixes.size())
    {
        value /= divisor;
        ++index;
    }
    if (index == 0) return std::format("{} {}", bytes, suffixes[0]);

    mut auto formatted = std::format("{:.2f}", value);
    while (formatted.back() == '0') formatted.pop_back();
    if (formatted.back() == '.') formatted.pop_back();
    return std::format("{} {}", formatted, suffixes[index]);
}
}  // namespace

[[nodiscard]] def format_bytes_binary(u64 bytes) -> std::string
{
    constexpr std::array<std::string_view, 7> suffixes{
        "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    return format_bytes_with(bytes, 1024.0, suffixes);
}

[[nodiscard]] def format_bytes_decimal(u64 bytes) -> std::string
{
    constexpr std::array<std::string_view, 7> suffixes{
        "B", "KB", "MB", "GB", "TB", "PB", "EB"};
    return format_bytes_with(bytes, 1000.0, suffixes);
}

[[nodiscard]] def hex_dump(std::span<const std::byte> bytes) -> std::string
{
    constexpr auto bytes_per_line = 16zu;

    mut std::string out{};
    out.reserve((bytes.size() / bytes_per_line + 1) * 80);

    for (mut auto offset = 0zu; offset < bytes.size(); offset += bytes_per_line)
    {
        std::format_to(std::back_inserter(out), "{:08x}  ", offset);

        for (mut auto i = 0zu; i < bytes_per_line; ++i)
        {
            if (offset + i < bytes.size())
            {
                std::format_to(
                    std::back_inserter(out),
                    "{:02x} ",
                    static_cast<u32>(bytes[offset + i]));
            }
            else
            {
                out += "   ";
            }
            if (i == 7) out += ' ';
        }

        out += " |";
        for (mut auto i = 0zu; i < bytes_per_line; ++i)
        {
            if (offset + i >= bytes.size()) break;
            const auto b = static_cast<u8>(bytes[offset + i]);
            out += (b >= 0x20 and b < 0x7f) ? static_cast<char>(b) : '.';
        }
        out += "|\n";
    }
    return out;
}
}  // namespace dans::fmt
