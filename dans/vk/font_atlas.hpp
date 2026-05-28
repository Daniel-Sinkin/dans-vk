// dans/vk/font_atlas.hpp
//
#pragma once

#include "dans/vk/types.hpp"
// StdLib
#include <filesystem>
#include <vector>
//

namespace dans::vk
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
};

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

}  // namespace dans::vk
