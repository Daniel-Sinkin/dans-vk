// dans/image/image.hpp
//
#pragma once

#include "dans/gfx/color.hpp"
// Externals
#include "dans/dans-core/types.hpp"
// StdLib
#include <filesystem>
#include <mdspan>
#include <span>
#include <vector>
//

namespace dans::image
{
using dans::gfx::Color;
using dans::gfx::ColorU8;

// 8-bit RGBA image. `storage` owns the pixels; `view()` is a row-major
// (height x width) matrix view into it. A live mdspan member would dangle on
// copy/move, so the view is rebuilt from the current storage on each call.
struct Image8
{
    u32 width{};
    u32 height{};
    std::vector<ColorU8> storage{};

    using View = std::mdspan<ColorU8, std::dextents<u32, 2>>;
    using ConstView = std::mdspan<const ColorU8, std::dextents<u32, 2>>;

    [[nodiscard]] auto view() noexcept -> View
    {
        return View{storage.data(), height, width};
    }
    [[nodiscard]] auto view() const noexcept -> ConstView
    {
        return ConstView{storage.data(), height, width};
    }
};

// 32-bit float RGBA image (HDR). Same storage/view contract as Image8.
struct ImageF
{
    u32 width{};
    u32 height{};
    std::vector<Color> storage{};

    using View = std::mdspan<Color, std::dextents<u32, 2>>;
    using ConstView = std::mdspan<const Color, std::dextents<u32, 2>>;

    [[nodiscard]] auto view() noexcept -> View
    {
        return View{storage.data(), height, width};
    }
    [[nodiscard]] auto view() const noexcept -> ConstView
    {
        return ConstView{storage.data(), height, width};
    }
};

[[nodiscard]] auto load_rgba8(const std::filesystem::path& path) -> Image8;
[[nodiscard]] auto load_rgba_f32(const std::filesystem::path& path) -> ImageF;

auto write_rgba8_png(
    const std::filesystem::path& path, u32 width, u32 height, std::span<const ColorU8> pixels
) -> void;

}  // namespace dans::image
