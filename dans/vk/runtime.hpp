#pragma once

#include "dans/vk/camera.hpp"
#include "dans/vk/debug_draw.hpp"
#include "dans/font/font_atlas.hpp"
#include "dans/vk/drawlist.hpp"
#include "dans/vk/mesh.hpp"
#include "dans/vk/shape_draw.hpp"
#include "dans/vk/text_draw.hpp"
#include "dans/vk/types.hpp"

#include <concepts>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace dans::vk
{

struct DescriptorIndexingSupport
{
    bool descriptor_indexing{};
    bool sampled_image_array_dynamic_indexing{};
    bool runtime_descriptor_array{};
    bool descriptor_binding_partially_bound{};
    bool sampled_image_non_uniform_indexing{};
    bool storage_buffer_non_uniform_indexing{};
    bool sampled_image_update_after_bind{};
    bool storage_buffer_update_after_bind{};
};

struct RuntimeStats
{
    f32 last_frame_ms{};
    f32 last_update_ms{};
    f32 last_ui_ms{};
    f32 last_render_ms{};
    u32 mesh_draws{};
    u32 mesh_batches{};
    u32 debug_segments{};
    u32 lights{};
};

enum class RenderMode : u8
{
    three_d = 0,
    two_d = 1,
};

struct RuntimeConfig
{
    std::string window_title{"dans_vk app"};
    u32 initial_width{1280};
    u32 initial_height{800};
    std::filesystem::path shader_dir{};
    std::filesystem::path screenshot_path{};
    u32 smoke_frames{};
    bool hide_ui{};
    bool transparent_screenshot{};
    bool enable_validation{true};
    Color clear_color{0.035f, 0.045f, 0.055f, 1.0f};
    u32 shadow_map_resolution{2048};
    RenderMode render_mode{RenderMode::three_d};
};

struct TextureLoadConfig
{
    bool srgb{true};
};

struct HdrTextureLoadConfig
{
    f32 exposure{1.0f};
};

struct MeshReserveConfig
{
    MeshHandle mesh{};
    usize vertex_capacity{};
    usize index_capacity{};
    MeshVertexFormat vertex_format{MeshVertexFormat::standard};
};

struct MeshUpdateConfig
{
    bool validate_indices{};
};

struct KeyboardModifiers
{
    bool shift{};
    bool control{};
    bool alt{};
    bool super{};
};

// Convention: every *_logical_px coordinate is in logical pixels (SDL window
// space), matching ImGui and the 2D screen-space projection. Multiply by
// Runtime::dpi_scale() only when you need physical framebuffer pixels;
// Runtime::framebuffer_extent() already gives the physical size directly.
struct MouseClick
{
    bool occurred{};
    Vec2 position_logical_px{};
    u8 click_count{};
    KeyboardModifiers modifiers{};
};

struct InputState
{
    Vec2 mouse_logical_px{};
    bool mouse_captured_by_ui{};
    bool space_pressed{};
    bool key_g_pressed{};
    bool key_r_pressed{};
    bool key_s_pressed{};
    bool key_x_pressed{};
    bool key_y_pressed{};
    bool key_z_pressed{};
    bool key_c_pressed{};
    bool key_n_pressed{};
    bool key_l_pressed{};
    bool key_m_pressed{};
    bool key_o_pressed{};
    bool key_p_pressed{};
    bool key_left_pressed{};
    bool key_right_pressed{};
    bool key_enter_pressed{};
    bool key_delete_pressed{};
    bool key_plus_pressed{};
    bool key_minus_pressed{};
    bool left_button_down{};
    bool right_button_down{};
    bool shift_held{};
    bool control_held{};
    bool alt_held{};
    bool super_held{};
    MouseClick left_click{};
    MouseClick right_click{};
};

struct FrameContext
{
    VkInstance instance{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkQueue graphics_queue{VK_NULL_HANDLE};
    u32 graphics_queue_family{};
    VkCommandBuffer command_buffer{VK_NULL_HANDLE};
    VkRenderPass main_render_pass{VK_NULL_HANDLE};
    VmaAllocator allocator{VK_NULL_HANDLE};
    VkExtent2D extent{};
    u32 frame_index{};
    u32 swapchain_image_index{};
    u32 swapchain_image_count{};
    f32 dt_seconds{};
    Camera& camera;
    DrawList& draw;
    const InputState& input;
    const DescriptorIndexingSupport& descriptor_indexing;
    const RuntimeStats& stats;
};

class Runtime;

namespace detail
{
template <typename App>
concept has_setup = requires(App& app, Runtime& runtime) {
    { app.setup(runtime) } -> std::same_as<void>;
};

template <typename App>
concept has_update = requires(App& app, FrameContext& frame, f32 dt_seconds) {
    { app.update(frame, dt_seconds) } -> std::same_as<void>;
};

template <typename App>
concept has_draw_ui = requires(App& app, FrameContext& frame) {
    { app.draw_ui(frame) } -> std::same_as<void>;
};

template <typename App>
concept has_shutdown = requires(App& app, Runtime& runtime) {
    { app.shutdown(runtime) } -> std::same_as<void>;
};

template <typename App>
concept has_runtime_hook =
    has_setup<App> or has_update<App> or has_draw_ui<App> or has_shutdown<App>;
}  // namespace detail

class Runtime
{
  public:
    explicit Runtime(RuntimeConfig = {});
    ~Runtime();

    Runtime(const Runtime&) = delete;
    auto operator=(const Runtime&) -> Runtime& = delete;
    Runtime(Runtime&&) noexcept;
    auto operator=(Runtime&&) noexcept -> Runtime&;

    // clang-format off
    auto initialize()                -> void;
    auto shutdown() noexcept         -> void;

    [[nodiscard]] auto begin_frame() -> FrameContext*;
    [[nodiscard]] auto frame()       -> FrameContext&;
    [[nodiscard]] auto frame() const -> const FrameContext&;

    auto draw_runtime_ui()    -> void;
    auto render_shadow_pass() -> void;
    auto begin_main_pass()    -> void;
    auto render_draw_list()   -> void;
    auto render_imgui()       -> void;
    auto end_main_pass()      -> void;
    auto end_frame()          -> void;

    [[nodiscard]] auto ui_visible() const noexcept -> bool;

    [[nodiscard]] auto upload_mesh(const MeshData&)                        -> MeshHandle;
    [[nodiscard]] auto upload_mesh(const PositionNormalMeshData&)          -> MeshHandle;
    [[nodiscard]] auto upload_mesh(const QuantizedPositionNormalMeshData&) -> MeshHandle;
    [[nodiscard]] auto reserve_mesh_capacity(const MeshReserveConfig&)     -> MeshHandle;

    // Reuses existing buffers when capacity permits. Callers must avoid updating a handle
    // still used by in-flight command buffers.
    [[nodiscard]] auto update_mesh(MeshHandle, const MeshData&, const MeshUpdateConfig& = {})                        -> MeshHandle;
    [[nodiscard]] auto update_mesh(MeshHandle, const PositionNormalMeshData&, const MeshUpdateConfig& = {})          -> MeshHandle;
    [[nodiscard]] auto update_mesh(MeshHandle, const QuantizedPositionNormalMeshData&, const MeshUpdateConfig& = {}) -> MeshHandle;

    [[nodiscard]] auto replace_mesh(MeshHandle, const MeshData&) -> MeshHandle;
    [[nodiscard]] auto load_texture(const std::filesystem::path&, const TextureLoadConfig& = {}) -> TextureHandle;
    [[nodiscard]] auto load_hdr_texture(const std::filesystem::path&, const HdrTextureLoadConfig& = {}) -> TextureHandle;
    [[nodiscard]] auto upload_texture_rgba(std::span<const ColorU8>, u32 width, u32 height, const TextureLoadConfig& = {}) -> TextureHandle;
    [[nodiscard]] auto imgui_texture_id(TextureHandle) -> uptr;
    auto load_font(const dans::font::FontBakeConfig&) -> void;
    [[nodiscard]] auto font() const noexcept -> const dans::font::BakedFont&;
    [[nodiscard]] auto font_loaded() const noexcept -> bool;
    auto request_screenshot(std::filesystem::path path, bool transparent = false) -> void;

    auto camera(const CameraConfig&)                       noexcept -> Camera&;
    [[nodiscard]] auto camera()                            noexcept -> Camera&;
    [[nodiscard]] auto camera()                      const noexcept -> const Camera&;

    [[nodiscard]] auto render_mode()                 const noexcept -> RenderMode;
    [[nodiscard]] auto camera_2d_pivot()             const noexcept -> Vec2;
    [[nodiscard]] auto camera_2d_zoom()              const noexcept -> f32;
    auto set_camera_2d(Vec2 pivot, f32 zoom)               noexcept -> void;
    [[nodiscard]] auto screen_to_world_2d(Vec2 pixel) const noexcept -> Vec2;
    [[nodiscard]] auto framebuffer_extent()          const noexcept -> Vec2;
    [[nodiscard]] auto logical_extent()              const noexcept -> Vec2;
    [[nodiscard]] auto dpi_scale()                   const noexcept -> f32;
    [[nodiscard]] auto stats()                       const noexcept -> const RuntimeStats&;
    [[nodiscard]] auto descriptor_indexing_support() const noexcept -> const DescriptorIndexingSupport&;
    // clang-format on

    template <typename App>
    [[nodiscard]]
    auto run_prototype(App& app) -> int
    {
        static_assert(
            detail::has_runtime_hook<App>,
            "dans_vk prototype apps must provide at least one of setup(Runtime&), "
            "update(FrameContext&, f32), draw_ui(FrameContext&), or shutdown(Runtime&)."
        );

        initialize();
        if constexpr (detail::has_setup<App>)
        {
            app.setup(*this);
        }

        while (auto* current_frame = begin_frame())
        {
            if constexpr (detail::has_update<App>)
            {
                app.update(*current_frame, current_frame->dt_seconds);
            }

            if (ui_visible())
            {
                draw_runtime_ui();
                if constexpr (detail::has_draw_ui<App>)
                {
                    app.draw_ui(*current_frame);
                }
            }

            render_shadow_pass();
            begin_main_pass();
            render_draw_list();
            render_imgui();
            end_main_pass();
            end_frame();
        }

        if constexpr (detail::has_shutdown<App>)
        {
            app.shutdown(*this);
        }
        return 0;
    }

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace dans::vk
