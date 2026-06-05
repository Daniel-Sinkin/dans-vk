// dans/font/font_atlas.cpp
//
#include "dans/font/font_atlas.hpp"

#include "dans/dans-util/dans_util.hpp"
// Externals
#include <stb_truetype.h>
// StdLib
#include <cstdio>
#include <format>
#include <string>
#include <string_view>
//

namespace dans::font
{

namespace
{

[[nodiscard]] auto read_file_bytes(const std::filesystem::path& path) -> std::vector<u8>
{
    const auto fail = [&](std::string_view action) -> std::string
    { return std::format("failed to {} font file: {}", action, path.string()); };

    std::FILE* file = std::fopen(path.string().c_str(), "rb");
    if (file == nullptr) DANS_PANIC(fail("open"));
    if (std::fseek(file, 0, SEEK_END) != 0) DANS_PANIC(fail("seek"));
    const auto end = std::ftell(file);
    if (end < 0) DANS_PANIC(fail("size"));
    if (std::fseek(file, 0, SEEK_SET) != 0) DANS_PANIC(fail("rewind"));

    std::vector<u8> data(static_cast<usize>(end));
    const auto read = std::fread(data.data(), 1, data.size(), file);
    std::fclose(file);
    if (read != data.size()) DANS_PANIC(fail("read"));
    return data;
}

}  // namespace

auto FontBakeConfig::to_string() const -> std::string
{
    return std::format(
        "FontBakeConfig{{ttf_path={}, pixel_size={}, dpi_scale={}, first_codepoint={}, "
        "codepoint_count={}, atlas={}x{}}}",
        ttf_path.string(),
        pixel_size,
        dpi_scale,
        first_codepoint,
        codepoint_count,
        atlas_width,
        atlas_height
    );
}

auto bake_font(const FontBakeConfig& config) -> BakedFont
{
    {  // Expects
        if (not is_valid(config))
        {
            DANS_PANIC(std::format("{}: {}", to_string(validate(config)), config.to_string()));
        }
    }

    const auto baked_pixel_size = config.pixel_size * config.dpi_scale;

    const auto ttf_bytes = read_file_bytes(config.ttf_path);
    BakedFont result = [&]
    {
        stbtt_fontinfo font_info{};
        const auto offset = stbtt_GetFontOffsetForIndex(ttf_bytes.data(), 0);
        if (offset < 0 or stbtt_InitFont(&font_info, ttf_bytes.data(), offset) == 0)
        {
            DANS_PANIC(std::format("failed to parse {}", config.ttf_path.string()));
        }

        BakedFont out{};
        out.metrics = [&]
        {
            FontMetrics metrics{};
            int ascent{};
            int descent{};
            int line_gap{};
            stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);
            const auto scale = stbtt_ScaleForPixelHeight(&font_info, baked_pixel_size);
            const auto adjust = scale / config.dpi_scale;
            metrics.ascent = static_cast<f32>(ascent) * adjust;
            metrics.descent = static_cast<f32>(descent) * adjust;
            metrics.line_gap = static_cast<f32>(line_gap) * adjust;
            metrics.pixel_size = config.pixel_size;
            return metrics;
        }();
        out.atlas_width = config.atlas_width;
        out.atlas_height = config.atlas_height;
        out.first_codepoint = config.first_codepoint;
        out.pixels.resize(
            static_cast<usize>(config.atlas_width) * static_cast<usize>(config.atlas_height), u8{0}
        );
        out.glyphs.reserve(config.codepoint_count);
        return out;
    }();

    const auto chardata = [&]
    {
        std::vector<stbtt_bakedchar> out(config.codepoint_count);
        const auto bake_result = stbtt_BakeFontBitmap(
            ttf_bytes.data(),
            0,
            baked_pixel_size,
            result.pixels.data(),
            static_cast<int>(config.atlas_width),
            static_cast<int>(config.atlas_height),
            static_cast<int>(config.first_codepoint),
            static_cast<int>(config.codepoint_count),
            out.data()
        );
        if (bake_result > 0) return out;
        DANS_PANIC(std::format("font atlas could not fit the glyphs: {}", config.to_string()));
    }();

    for (const auto& src : chardata)
    {
        const auto inv_dpi = 1.0f / config.dpi_scale;
        const auto atlas_x = static_cast<u16>(src.x0);
        const auto atlas_y = static_cast<u16>(src.y0);
        const auto atlas_w = static_cast<u16>(src.x1 - atlas_x);
        const auto atlas_h = static_cast<u16>(src.y1 - atlas_y);
        result.glyphs.push_back({
            .atlas_x = atlas_x,
            .atlas_y = atlas_y,
            .atlas_w = atlas_w,
            .atlas_h = atlas_h,
            .width = static_cast<f32>(atlas_w) * inv_dpi,
            .height = static_cast<f32>(atlas_h) * inv_dpi,
            .offset_x = src.xoff * inv_dpi,
            .offset_y = src.yoff * inv_dpi,
            .advance = src.xadvance * inv_dpi,
        });
    }
    return result;
}

auto glyph_for(const BakedFont& font, u32 codepoint) noexcept -> const GlyphMetrics*
{
    if (codepoint < font.first_codepoint) return nullptr;
    const auto index = static_cast<usize>(codepoint - font.first_codepoint);
    if (index >= font.glyphs.size()) return nullptr;
    return &font.glyphs[index];
}

auto line_advance(const FontMetrics& metrics) noexcept -> f32
{
    return metrics.ascent - metrics.descent + metrics.line_gap;
}

}  // namespace dans::font
