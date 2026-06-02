// dans/vk/drawlist.hpp
//
#pragma once

#include "dans/vk/debug_draw.hpp"
#include "dans/vk/mesh.hpp"
#include "dans/vk/shape_draw.hpp"
#include "dans/vk/text_draw.hpp"
#include "dans/vk/types.hpp"
// StdLib
#include <span>
#include <vector>
//

namespace dans::vk
{

struct MaterialTextures
{
    TextureHandle base_color{};
};

struct Material
{
    Color base_color{Color::white};
    Color emissive_color{Color::black};
    f32 metallic{0.0f};
    f32 roughness{0.55f};
    f32 ambient_occlusion{1.0f};
    MaterialTextures textures{};
};

enum class MeshDebugMode : u8
{
    none = 0,
    color_override = 1,
    selected_pulse = 2,
    scalar_heatmap = 3,
    normal = 4,
    object_id = 5,
    camera_depth = 6,
    triangle_selected_pulse = 7,
    world_z_ramp = 8,
    facet_color = 9,
    angle_shaded = 10,
};

struct MeshDebugConfig
{
    MeshDebugMode mode{MeshDebugMode::none};
    Color color{1.0f, 0.0f, 1.0f, 0.85f};
    f32 scalar{};
    Vec2 scalar_range{0.0f, 1.0f};
    bool selected{};
    bool hidden{};
};

struct MeshRenderMask
{
    bool visible_to_camera{true};
    bool shadow_producer{true};
    bool shadow_consumer{true};
    bool light_receiver{true};
};

struct MeshDrawConfig
{
    MeshHandle mesh{};
    ObjectId object_id{};
    Transform transform{};
    Material material{};
    MeshRenderMask mask{};
    MeshDebugConfig debug{};
};

struct BasicMeshDrawConfig
{
    MeshHandle mesh{};
    ObjectId object_id{};
    Transform transform{};
    Color color{Color::white};
    MeshRenderMask mask{};
    MeshDebugConfig debug{};
};

struct MeshDrawCommand
{
    MeshHandle mesh{};
    ObjectId object_id{};
    Transform transform{};
    Material material{};
    MeshRenderMask mask{};
    MeshDebugConfig debug{};
};

enum class LightType : u8
{
    directional = 0,
    radial = 1,
    spot = 2,
};

struct LightShadowConfig
{
    bool enabled{};
    f32 bias{0.0025f};
    f32 strength{0.70f};
    f32 near_plane{0.05f};
    f32 far_plane{28.0f};
    f32 ortho_extent{7.0f};
};

struct LightConfig
{
    LightType type{LightType::directional};
    Vec3 position{0.0f, 0.0f, 2.0f};
    Vec3 direction{-0.45f, -0.35f, -0.82f};
    Color color{Color::white};
    f32 intensity{1.0f};
    f32 range{6.0f};
    f32 inner_cone_angle{glm::radians(12.0f)};
    f32 outer_cone_angle{glm::radians(24.0f)};
    LightShadowConfig shadow{};
    bool enabled{true};
};

struct EnvironmentConfig
{
    TextureHandle texture{};
    f32 lighting_intensity{0.0f};
    f32 background_intensity{0.0f};
    f32 rotation_radians{};
    Color background_color{0.14f, 0.16f, 0.18f, 0.0f};
    Color background_top_color{0.36f, 0.43f, 0.48f, 0.0f};
    bool gradient_background{};
    bool visible_to_camera{true};
};

struct DirectionalLightConfig
{
    Vec3 direction{-0.45f, -0.35f, -0.82f};
    Color color{Color::white};
    f32 intensity{1.0f};
    LightShadowConfig shadow{};
    bool enabled{true};
};

struct RadialLightConfig
{
    Vec3 position{0.0f, 0.0f, 2.0f};
    Color color{Color::white};
    f32 intensity{8.0f};
    f32 range{5.0f};
    bool enabled{true};
};

struct SpotLightConfig
{
    Vec3 position{0.0f, 0.0f, 3.0f};
    Vec3 direction{-k_axis_z};
    Color color{Color::white};
    f32 intensity{18.0f};
    f32 range{7.0f};
    f32 inner_cone_angle{glm::radians(12.0f)};
    f32 outer_cone_angle{glm::radians(24.0f)};
    LightShadowConfig shadow{};
    bool enabled{true};
};

class DrawList
{
  public:
    auto clear() -> void;
    auto set_ambient_light(Color color) -> void;
    // clang-format off
    auto draw_mesh        (const MeshDrawConfig&        ) -> void;
    auto draw_basic_mesh  (const BasicMeshDrawConfig&   ) -> void;
    auto debug_line       (const DebugLineConfig&       ) -> void;
    auto debug_arrow      (const DebugArrowConfig&      ) -> void;
    auto debug_sphere     (const DebugSphereConfig&     ) -> void;
    auto text             (const TextDrawConfig&        ) -> void;
    auto text_screen      (const TextScreenConfig&      ) -> void;
    auto rect             (const RectConfig&            ) -> void;
    auto circle           (const CircleConfig&          ) -> void;
    auto line_2d          (const Line2DConfig&          ) -> void;
    auto sector           (const SectorConfig&          ) -> void;
    auto bezier           (const BezierConfig&          ) -> void;
    auto add_light        (const LightConfig&           ) -> void;
    auto directional_light(const DirectionalLightConfig&) -> void;
    auto radial_light     (const RadialLightConfig&     ) -> void;
    auto spot_light       (const SpotLightConfig&       ) -> void;
    auto set_environment  (const EnvironmentConfig&     ) -> void;

    [[nodiscard]] auto mesh_commands()         const noexcept -> std::span<const MeshDrawCommand>;
    [[nodiscard]] auto debug_segments()        const noexcept -> std::span<const DebugSegment>;
    [[nodiscard]] auto debug_on_top_segments() const noexcept -> std::span<const DebugSegment>;
    [[nodiscard]] auto world_text_commands()   const noexcept -> std::span<const TextDrawCommand>;
    [[nodiscard]] auto screen_text_commands()  const noexcept -> std::span<const TextDrawCommand>;
    [[nodiscard]] auto world_shapes()          const noexcept -> std::span<const Shape2DInstance>;
    [[nodiscard]] auto screen_shapes()         const noexcept -> std::span<const Shape2DInstance>;
    [[nodiscard]] auto lights()                const noexcept -> std::span<const LightConfig>;
    [[nodiscard]] auto ambient_light()         const noexcept -> Color;
    [[nodiscard]] auto environment()           const noexcept -> const EnvironmentConfig&;
    // clang-format on

  private:
    std::vector<MeshDrawCommand> mesh_commands_{};
    std::vector<DebugSegment> debug_segments_{};
    std::vector<DebugSegment> debug_on_top_segments_{};
    std::vector<TextDrawCommand> world_text_commands_{};
    std::vector<TextDrawCommand> screen_text_commands_{};
    std::vector<Shape2DInstance> world_shapes_{};
    std::vector<Shape2DInstance> screen_shapes_{};
    std::vector<LightConfig> lights_{};
    Color ambient_light_{0.035f, 0.040f, 0.050f, 1.0f};
    EnvironmentConfig environment_{};
};

}  // namespace dans::vk
