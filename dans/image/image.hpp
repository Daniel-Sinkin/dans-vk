// dans/image/image.hpp
//
#pragma once

#include "dans/dans-core/types.hpp"
// StdLib
#include <filesystem>
#include <span>
#include <vector>
//

namespace dans::image
{

struct Image8
{
    u32 width{};
    u32 height{};
    std::vector<u8> rgba{};
};

struct ImageF
{
    u32 width{};
    u32 height{};
    std::vector<f32> rgba{};
};

[[nodiscard]] auto load_rgba8(const std::filesystem::path& path) -> Image8;
[[nodiscard]] auto load_rgba_f32(const std::filesystem::path& path) -> ImageF;

auto write_rgba8_png(
    const std::filesystem::path& path, u32 width, u32 height, std::span<const u8> rgba
) -> void;

}  // namespace dans::image
