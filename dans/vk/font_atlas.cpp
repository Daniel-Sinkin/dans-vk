// dans/vk/font_atlas.cpp
//
#include "dans/vk/font_atlas.hpp"
// Externals
#include <stb_truetype.h>
// StdLib
#include <cstdio>
#include <format>
#include <stdexcept>
//

namespace dans::vk
{

namespace
{

[[nodiscard]] auto read_file_bytes(const std::filesystem::path& path) -> std::vector<u8>
{
    std::FILE* file = std::fopen(path.string().c_str(), "rb");
    if (file == nullptr)
    {
        throw std::runtime_error(std::format("failed to open font file: {}", path.string()));
    }

    if (std::fseek(file, 0, SEEK_END) != 0)
    {
        std::fclose(file);
        throw std::runtime_error(std::format("failed to seek font file: {}", path.string()));
    }
    const auto end = std::ftell(file);
    if (end < 0)
    {
        std::fclose(file);
        throw std::runtime_error(std::format("failed to size font file: {}", path.string()));
    }
    std::rewind(file);

    std::vector<u8> data(static_cast<usize>(end));
    const auto read = std::fread(data.data(), 1, data.size(), file);
    std::fclose(file);
    if (read != data.size())
    {
        throw std::runtime_error(std::format("failed to read font file: {}", path.string()));
    }
    return data;
}

}  // namespace

auto bake_font(const FontBakeConfig& config) -> BakedFont
{
    if (config.codepoint_count == 0u)
    {
        throw std::runtime_error("bake_font: codepoint_count must be > 0");
    }
    if (config.atlas_width == 0u or config.atlas_height == 0u)
    {
        throw std::runtime_error("bake_font: atlas dimensions must be > 0");
    }
    if (config.pixel_size <= 0.0f)
    {
        throw std::runtime_error("bake_font: pixel_size must be positive");
    }
    const auto dpi_scale = (config.dpi_scale > 0.0f) ? config.dpi_scale : 1.0f;
    const auto inv_dpi = 1.0f / dpi_scale;
    const auto baked_pixel_size = config.pixel_size * dpi_scale;

    const auto ttf_bytes = read_file_bytes(config.ttf_path);

    stbtt_fontinfo info{};
    const auto offset = stbtt_GetFontOffsetForIndex(ttf_bytes.data(), 0);
    if (offset < 0 or stbtt_InitFont(&info, ttf_bytes.data(), offset) == 0)
    {
        throw std::runtime_error(
            std::format("bake_font: stb_truetype failed to parse {}", config.ttf_path.string())
        );
    }

    BakedFont result{};
    result.atlas_width = config.atlas_width;
    result.atlas_height = config.atlas_height;
    result.first_codepoint = config.first_codepoint;
    result.pixels.assign(
        static_cast<usize>(config.atlas_width) * static_cast<usize>(config.atlas_height), u8{0}
    );
    result.glyphs.resize(config.codepoint_count);

    std::vector<stbtt_bakedchar> chardata(config.codepoint_count);
    const auto bake_result = stbtt_BakeFontBitmap(
        ttf_bytes.data(),
        0,
        baked_pixel_size,
        result.pixels.data(),
        static_cast<int>(config.atlas_width),
        static_cast<int>(config.atlas_height),
        static_cast<int>(config.first_codepoint),
        static_cast<int>(config.codepoint_count),
        chardata.data()
    );
    if (bake_result <= 0)
    {
        throw std::runtime_error(std::format(
            "bake_font: atlas {}x{} could not fit {} glyphs at {}px (dpi_scale {})",
            config.atlas_width,
            config.atlas_height,
            config.codepoint_count,
            static_cast<f64>(config.pixel_size),
            static_cast<f64>(dpi_scale)
        ));
    }

    int ascent{};
    int descent{};
    int line_gap{};
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    const auto scale = stbtt_ScaleForPixelHeight(&info, baked_pixel_size);
    result.metrics.ascent = static_cast<f32>(ascent) * scale * inv_dpi;
    result.metrics.descent = static_cast<f32>(descent) * scale * inv_dpi;
    result.metrics.line_gap = static_cast<f32>(line_gap) * scale * inv_dpi;
    result.metrics.pixel_size = config.pixel_size;

    for (auto i = 0zu; i < chardata.size(); ++i)
    {
        const auto& src = chardata[i];
        GlyphMetrics& dst = result.glyphs[i];
        dst.atlas_x = static_cast<u16>(src.x0);
        dst.atlas_y = static_cast<u16>(src.y0);
        dst.atlas_w = static_cast<u16>(src.x1 - src.x0);
        dst.atlas_h = static_cast<u16>(src.y1 - src.y0);
        dst.width = static_cast<f32>(dst.atlas_w) * inv_dpi;
        dst.height = static_cast<f32>(dst.atlas_h) * inv_dpi;
        dst.offset_x = src.xoff * inv_dpi;
        dst.offset_y = src.yoff * inv_dpi;
        dst.advance = src.xadvance * inv_dpi;
    }
    return result;
}

auto glyph_for(const BakedFont& font, u32 codepoint) noexcept -> const GlyphMetrics*
{
    if (codepoint < font.first_codepoint)
    {
        return nullptr;
    }
    const auto index = static_cast<usize>(codepoint - font.first_codepoint);
    if (index >= font.glyphs.size())
    {
        return nullptr;
    }
    return &font.glyphs[index];
}

auto line_advance(const FontMetrics& metrics) noexcept -> f32
{
    return metrics.ascent - metrics.descent + metrics.line_gap;
}

}  // namespace dans::vk
