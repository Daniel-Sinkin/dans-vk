// dans/image/image.cpp
//
#include "dans/image/image.hpp"
// Externals
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
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
    Image8 image{.width = static_cast<u32>(width), .height = static_cast<u32>(height)};
    const auto count = static_cast<usize>(width) * static_cast<usize>(height) * 4zu;
    image.rgba.assign(pixels, pixels + count);
    stbi_image_free(pixels);
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
        throw std::runtime_error(std::format(
            "dans::image: failed to load HDR {}: {}", path.string(), stbi_failure_reason()
        ));
    }
    ImageF image{.width = static_cast<u32>(width), .height = static_cast<u32>(height)};
    const auto count = static_cast<usize>(width) * static_cast<usize>(height) * 4zu;
    image.rgba.assign(pixels, pixels + count);
    stbi_image_free(pixels);
    return image;
}

auto write_rgba8_png(
    const std::filesystem::path& path, u32 width, u32 height, std::span<const u8> rgba
) -> void
{
    const auto expected = static_cast<usize>(width) * static_cast<usize>(height) * 4zu;
    if (width == 0u or height == 0u or rgba.size() != expected)
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
        rgba.data(),
        static_cast<int>(width * 4u)
    );
    if (result == 0)
    {
        throw std::runtime_error(std::format("dans::image: failed to write PNG {}", path.string()));
    }
}

}  // namespace dans::image
