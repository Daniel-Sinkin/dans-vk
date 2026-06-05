// dans/mesh/mesh.cpp
//
#include "dans/mesh/mesh.hpp"
// Externals
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
// StdLib
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <ranges>
#include <span>
//

namespace dans::mesh
{

auto Transform::matrix() const noexcept -> Mat4
{
    const auto translation_matrix = glm::translate(Mat4{1.0f}, translation);
    const auto rotation_matrix = glm::mat4_cast(rotation);
    const auto scale_matrix = glm::scale(Mat4{1.0f}, scale);
    return translation_matrix * rotation_matrix * scale_matrix;
}

namespace
{
// Unit (side length 1) geometry, built at compile time. The runtime make_*
// wrappers scale these positions by the requested side length and apply color.
[[nodiscard]] constexpr auto build_unit_quad_vertices() -> std::array<Vertex, 4>
{
    constexpr auto h = 0.5f;
    constexpr std::array<Vec2, 4> xy{{{-h, -h}, {h, -h}, {h, h}, {-h, h}}};
    constexpr std::array<Vec2, 4> uv{{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}};
    std::array<Vertex, 4> vertices{};
    for (auto k = 0zu; k < vertices.size(); ++k)
    {
        vertices[k] = Vertex{
            .position = {xy[k].x, xy[k].y, 0.0f},
            .normal = k_axis_z,
            .color = Color::white,
            .texcoord = uv[k],
        };
    }
    return vertices;
}

[[nodiscard]] constexpr auto build_unit_cube_vertices() -> std::array<Vertex, 24>
{
    constexpr auto h = 0.5f;
    // Corner index is (x << 2) | (y << 1) | z, with bit 0 = -h and 1 = +h.
    constexpr std::array<Vec3, 8> corner{{
        {-h, -h, -h},
        {-h, -h, h},
        {-h, h, -h},
        {-h, h, h},
        {h, -h, -h},
        {h, -h, h},
        {h, h, -h},
        {h, h, h},
    }};
    struct Face
    {
        std::array<u32, 4> corners;
        Vec3 normal;
    };
    constexpr std::array<Face, 6> faces{{
        Face{.corners = {0b001u, 0b101u, 0b111u, 0b011u}, .normal = k_axis_z},
        Face{.corners = {0b100u, 0b000u, 0b010u, 0b110u}, .normal = -k_axis_z},
        Face{.corners = {0b101u, 0b100u, 0b110u, 0b111u}, .normal = k_axis_x},
        Face{.corners = {0b000u, 0b001u, 0b011u, 0b010u}, .normal = -k_axis_x},
        Face{.corners = {0b011u, 0b111u, 0b110u, 0b010u}, .normal = k_axis_y},
        Face{.corners = {0b000u, 0b100u, 0b101u, 0b001u}, .normal = -k_axis_y},
    }};
    constexpr std::array<Vec2, 4> uvs{{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}};

    std::array<Vertex, 24> vertices{};
    auto index = 0zu;
    for (const auto& face : faces)
    {
        for (const auto& [face_corner, uv] : std::views::zip(face.corners, uvs))
        {
            vertices[index++] = Vertex{
                .position = corner[face_corner],
                .normal = face.normal,
                .color = Color::white,
                .texcoord = uv,
            };
        }
    }
    return vertices;
}

[[nodiscard]] constexpr auto build_cube_indices() -> std::array<u32, 36>
{
    constexpr std::array<u32, 6> face_pattern{{0u, 1u, 2u, 0u, 2u, 3u}};
    std::array<u32, 36> indices{};
    auto index = 0zu;
    for (auto face = 0u; face < 6u; ++face)
    {
        for (const auto offset : face_pattern)
        {
            indices[index++] = face * 4u + offset;
        }
    }
    return indices;
}

constexpr auto k_unit_quad_vertices = build_unit_quad_vertices();
constexpr std::array<u32, 6> k_unit_quad_indices{{0u, 1u, 2u, 0u, 2u, 3u}};
constexpr auto k_unit_cube_vertices = build_unit_cube_vertices();
constexpr auto k_unit_cube_indices = build_cube_indices();

[[nodiscard]] auto scaled_colored_mesh(
    std::span<const Vertex> unit_vertices,
    std::span<const u32> indices,
    f32 side_length,
    Color color
) -> MeshData
{
    const auto scale = std::max(0.0f, side_length);
    MeshData mesh{};
    mesh.vertices.reserve(unit_vertices.size());
    for (const auto& vertex : unit_vertices)
    {
        mesh.vertices.push_back(
            Vertex{
                .position = vertex.position * scale,
                .normal = vertex.normal,
                .color = color,
                .texcoord = vertex.texcoord,
            }
        );
    }
    mesh.indices.assign(indices.begin(), indices.end());
    return mesh;
}
}  // namespace

auto make_quad(f32 side_length, Color color) -> MeshData
{
    return scaled_colored_mesh(k_unit_quad_vertices, k_unit_quad_indices, side_length, color);
}

auto make_cube(f32 side_length, Color color) -> MeshData
{
    return scaled_colored_mesh(k_unit_cube_vertices, k_unit_cube_indices, side_length, color);
}

auto make_uv_sphere(const UvSphereConfig& config) -> MeshData
{
    const auto slices = std::max(3u, config.slices);
    const auto stacks = std::max(2u, config.stacks);
    const auto safe_radius = std::max(0.0f, config.radius);
    MeshData mesh{};

    const auto n_vertices = static_cast<usize>(slices + 1u) * static_cast<usize>(stacks + 1u);
    const auto n_indices = static_cast<usize>(slices) * static_cast<usize>(stacks) * 6zu;

    mesh.vertices.reserve(n_vertices);
    mesh.indices.reserve(n_indices);

    for (u32 stack = 0; stack <= stacks; ++stack)
    {
        const auto v = static_cast<f32>(stack) / static_cast<f32>(stacks);
        const auto phi = std::numbers::pi_v<f32> * v;
        const auto sin_phi = std::sin(phi);
        const auto cos_phi = std::cos(phi);
        for (u32 slice = 0; slice <= slices; ++slice)
        {
            const auto u = static_cast<f32>(slice) / static_cast<f32>(slices);
            const auto theta = 2.0f * std::numbers::pi_v<f32> * u;
            const Vec3 normal{
                sin_phi * std::cos(theta),
                sin_phi * std::sin(theta),
                cos_phi,
            };
            mesh.vertices.push_back(
                Vertex{
                    .position = normal * safe_radius,
                    .normal = normal,
                    .color = config.color,
                    .texcoord = {u, v},
                }
            );
        }
    }

    for (u32 stack = 0; stack < stacks; ++stack)
    {
        for (u32 slice = 0; slice < slices; ++slice)
        {
            const auto row0 = stack * (slices + 1u);
            const auto row1 = (stack + 1u) * (slices + 1u);
            const auto a = row0 + slice;
            const auto b = row0 + slice + 1u;
            const auto c = row1 + slice;
            const auto d = row1 + slice + 1u;
            mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, d});
        }
    }

    return mesh;
}

auto aabb_of(const MeshData& mesh) -> Aabb
{
    if (mesh.vertices.empty())
    {
        return {};
    }

    Vec3 min_value{std::numeric_limits<f32>::max()};
    Vec3 max_value{std::numeric_limits<f32>::lowest()};
    for (const auto& vertex : mesh.vertices)
    {
        min_value = glm::min(min_value, vertex.position);
        max_value = glm::max(max_value, vertex.position);
    }
    return Aabb{.min = min_value, .max = max_value};
}

auto aabb_of(const PositionNormalMeshData& mesh) -> Aabb
{
    if (mesh.vertices.empty())
    {
        return {};
    }

    Vec3 min_value{std::numeric_limits<f32>::max()};
    Vec3 max_value{std::numeric_limits<f32>::lowest()};
    for (const auto& vertex : mesh.vertices)
    {
        min_value = glm::min(min_value, vertex.position);
        max_value = glm::max(max_value, vertex.position);
    }
    return Aabb{.min = min_value, .max = max_value};
}

auto aabb_of(const QuantizedPositionNormalMeshData& mesh) -> Aabb
{
    if (mesh.vertices.empty())
    {
        return {};
    }
    return Aabb{.min = mesh.decode_origin, .max = mesh.decode_origin + mesh.decode_extent};
}

auto triangle_count(const MeshData& mesh) noexcept -> usize
{
    return mesh.indices.size() / 3zu;
}

auto triangle_count(const PositionNormalMeshData& mesh) noexcept -> usize
{
    return mesh.indices.size() / 3zu;
}

auto triangle_count(const QuantizedPositionNormalMeshData& mesh) noexcept -> usize
{
    return mesh.indices.size() / 3zu;
}

auto has_valid_indices(const MeshData& mesh) noexcept -> bool
{
    return std::ranges::all_of(
        mesh.indices,
        [&](u32 index) -> bool { return static_cast<usize>(index) < mesh.vertices.size(); }
    );
}

auto has_valid_indices(const PositionNormalMeshData& mesh) noexcept -> bool
{
    return std::ranges::all_of(
        mesh.indices,
        [&](u32 index) -> bool { return static_cast<usize>(index) < mesh.vertices.size(); }
    );
}

auto has_valid_indices(const QuantizedPositionNormalMeshData& mesh) noexcept -> bool
{
    return std::ranges::all_of(
        mesh.indices,
        [&](u32 index) -> bool { return static_cast<usize>(index) < mesh.vertices.size(); }
    );
}
}  // namespace dans::mesh
