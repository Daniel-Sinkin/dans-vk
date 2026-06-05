// dans/image/image.cpp
//
#include "dans/image/image.hpp"

#include "dans/dans-util/dans_util.hpp"
// Externals
#include <stb_image.h>
#include <stb_image_write.h>
// StdLib
#include <format>
#include <stdexcept>
//

namespace dans::image
{

auto load_rgba8(const std::filesystem::path& path) -> Image8
{
    int width{};
    int height{};
    int channels{};
    auto* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr)
    {
        throw std::runtime_error(
            std::format("dans::image: failed to load {}: {}", path.string(), stbi_failure_reason())
        );
    }
    dev::Defer _{[&pixels] { stbi_image_free(pixels); }};

    Image8 image{.width = static_cast<u32>(width), .height = static_cast<u32>(height)};
    const auto count = static_cast<usize>(width) * static_cast<usize>(height);
    const auto* typed = reinterpret_cast<const ColorU8*>(pixels);
    image.storage.assign(typed, typed + count);
    return image;
}

auto load_rgba_f32(const std::filesystem::path& path) -> ImageF
{
    int width{};
    int height{};
    int channels{};
    auto* pixels = stbi_loadf(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr)
    {
        DANS_PANIC(std::format("failed to load HDR {}: {}", path.string(), stbi_failure_reason()));
    }
    dev::Defer _{[&pixels] { stbi_image_free(pixels); }};

    ImageF image{.width = static_cast<u32>(width), .height = static_cast<u32>(height)};
    const auto count = static_cast<usize>(width) * static_cast<usize>(height);
    const auto* typed = reinterpret_cast<const Color*>(pixels);
    image.storage.assign(typed, typed + count);
    return image;
}

auto write_rgba8_png(
    const std::filesystem::path& path, u32 width, u32 height, std::span<const ColorU8> pixels
) -> void
{
    const auto expected = static_cast<usize>(width) * static_cast<usize>(height);
    if (width == 0u or height == 0u or pixels.size() != expected)
    {
        throw std::runtime_error(
            std::format("dans::image: invalid RGBA8 image for {}", path.string())
        );
    }
    const auto result = stbi_write_png(
        path.string().c_str(),
        static_cast<int>(width),
        static_cast<int>(height),
        4,
        pixels.data(),
        static_cast<int>(width * 4u)
    );
    if (result == 0)
    {
        throw std::runtime_error(std::format("dans::image: failed to write PNG {}", path.string()));
    }
}

}  // namespace dans::image
