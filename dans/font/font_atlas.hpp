// dans/font/font_atlas.hpp
//
#pragma once

#include "dans/dans-core/types.hpp"
// StdLib
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
//

namespace dans::font
{

// atlas_* values are in atlas (texture) pixels and feed UV computations.
// width/height/offset_*/advance are in logical pixels and feed quad layout,
// matching the visual size the caller asked for regardless of HiDPI scaling.
struct GlyphMetrics
{
    u16 atlas_x{};
    u16 atlas_y{};
    u16 atlas_w{};
    u16 atlas_h{};
    f32 width{};
    f32 height{};
    f32 offset_x{};
    f32 offset_y{};
    f32 advance{};
    u32 _pad_{};
};

struct FontMetrics
{
    f32 ascent{};
    f32 descent{};
    f32 line_gap{};
    f32 pixel_size{};
};

struct FontBakeConfig
{
    std::filesystem::path ttf_path{};
    f32 pixel_size{16.0f};
    // Multiplier for atlas resolution. The atlas is actually baked at
    // pixel_size * dpi_scale physical pixels so glyphs stay crisp on
    // HiDPI displays, while glyph metrics stay in logical (pixel_size)
    // units. Usually Runtime::load_font fills this for you.
    f32 dpi_scale{1.0f};
    u32 first_codepoint{32u};
    u32 codepoint_count{96u};
    u32 atlas_width{512u};
    u32 atlas_height{512u};

    [[nodiscard]] auto to_string() const -> std::string;
};

enum class FontBakeConfigValidity : u8
{
    valid = 0,
    zero_codepoint_count,
    zero_atlas_dimension,
    non_positive_pixel_size,
    non_positive_dpi_scale,
};

[[nodiscard]] inline auto validate(const FontBakeConfig& cfg) noexcept -> FontBakeConfigValidity
{
    if (cfg.codepoint_count == 0u) return FontBakeConfigValidity::zero_codepoint_count;
    if (cfg.atlas_width == 0u or cfg.atlas_height == 0u)
    {
        return FontBakeConfigValidity::zero_atlas_dimension;
    }
    if (cfg.pixel_size <= 0.0f) return FontBakeConfigValidity::non_positive_pixel_size;
    if (cfg.dpi_scale <= 0.0f) return FontBakeConfigValidity::non_positive_dpi_scale;
    return FontBakeConfigValidity::valid;
}

[[nodiscard]] inline auto is_valid(const FontBakeConfig& cfg) noexcept -> bool
{
    return validate(cfg) == FontBakeConfigValidity::valid;
}

// clang-format off
[[nodiscard]] constexpr auto to_string(FontBakeConfigValidity v) noexcept -> std::string_view
{
    switch (v)
    {
        case FontBakeConfigValidity::valid:                   return "valid";
        case FontBakeConfigValidity::zero_codepoint_count:    return "zero_codepoint_count";
        case FontBakeConfigValidity::zero_atlas_dimension:    return "zero_atlas_dimension";
        case FontBakeConfigValidity::non_positive_pixel_size: return "non_positive_pixel_size";
        case FontBakeConfigValidity::non_positive_dpi_scale:  return "non_positive_dpi_scale";
    }
    return "unknown";
}
// clang-format on

struct BakedFont
{
    FontMetrics metrics{};
    std::vector<GlyphMetrics> glyphs{};
    std::vector<u8> pixels{};
    u32 first_codepoint{};
    u32 atlas_width{};
    u32 atlas_height{};
};

[[nodiscard]] auto bake_font(const FontBakeConfig& config) -> BakedFont;

[[nodiscard]] auto glyph_for(const BakedFont& font, u32 codepoint) noexcept
    -> const GlyphMetrics*;

[[nodiscard]] auto line_advance(const FontMetrics& metrics) noexcept -> f32;

}  // namespace dans::font
