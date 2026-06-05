// dans/geom/geometry.cpp
//
#include "dans/geom/geometry.hpp"

#include "dans/dans-core/development_markers.hpp"
#include "dans/dans-util/dans_util.hpp"
// StdLib
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
//

namespace dans::geom
{
namespace
{
struct FaceDistance
{
    f32 distance{};
    Vec3 normal{};
};

[[nodiscard]] auto aabb_normal_at(Vec3 position, Vec3 box_min, Vec3 box_max) noexcept -> Vec3
{
    const std::array face_distances{
        FaceDistance{.distance = std::abs(position.x - box_min.x), .normal = -k_axis_x},
        FaceDistance{.distance = std::abs(position.x - box_max.x), .normal = k_axis_x},
        FaceDistance{.distance = std::abs(position.y - box_min.y), .normal = -k_axis_y},
        FaceDistance{.distance = std::abs(position.y - box_max.y), .normal = k_axis_y},
        FaceDistance{.distance = std::abs(position.z - box_min.z), .normal = -k_axis_z},
        FaceDistance{.distance = std::abs(position.z - box_max.z), .normal = k_axis_z},
    };
    const auto best = std::ranges::min_element(
        face_distances,
        [](const FaceDistance& lhs, const FaceDistance& rhs) noexcept -> bool
        { return lhs.distance < rhs.distance; }
    );
    return best == face_distances.end() ? k_axis_z : best->normal;
}
}  // namespace

auto intersect_sphere(const Ray& ray, const Sphere& sphere) noexcept -> std::optional<f32>
{
    const auto oc = ray.origin - sphere.center;

    const auto radius = std::max(0.0f, sphere.radius);
    const auto a = glm::dot(ray.direction, ray.direction);

    const auto half_b = glm::dot(oc, ray.direction);
    const auto c = glm::dot(oc, oc) - radius * radius;

    const auto discriminant = half_b * half_b - a * c;
    if (discriminant < 0.0f) return std::nullopt;

    const auto root = std::sqrt(discriminant);
    const auto near_t = (-half_b - root) / a;
    if (near_t >= 0.0f) return near_t;

    const auto far_t = (-half_b + root) / a;
    if (far_t >= 0.0f) return far_t;

    return std::nullopt;
}

auto intersect_aabb(const Ray& ray, const Aabb& aabb) noexcept -> std::optional<f32>
{
    using dans::math::in_interval;
    const auto box_min = glm::min(aabb.min, aabb.max);
    const auto box_max = glm::max(aabb.min, aabb.max);

    auto t_min = 0.0f;
    auto t_max = std::numeric_limits<f32>::max();
    for (usize axis = 0; axis < 3zu; ++axis)
    {
        const auto origin = ray.origin[axis];
        const auto direction = ray.direction[axis];
        if (std::abs(direction) <= 1.0e-8f)
        {
            if (not in_interval(origin, box_min[axis], box_max[axis])) return std::nullopt;
            continue;
        }
        const auto inv_direction = 1.0f / direction;
        auto t0 = (box_min[axis] - origin) * inv_direction;
        auto t1 = (box_max[axis] - origin) * inv_direction;
        if (t0 > t1) std::swap(t0, t1);
        t_min = std::max(t_min, t0);
        t_max = std::min(t_max, t1);
        if (t_max < t_min) return std::nullopt;
    }
    return t_min;
}

auto intersect_obb(const Ray& ray, const Obb& obb) noexcept -> std::optional<f32>
{
    if (const auto hit_res = hit_obb(ray, obb); hit_res)
    {
        return hit_res->distance;
    }
    return std::nullopt;
}

auto intersect_capsule(const Ray& ray, const Capsule& capsule) noexcept -> std::optional<f32>
{
    if (const auto hit_res = hit_capsule(ray, capsule); hit_res) return hit_res->distance;
    return std::nullopt;
}

auto hit_sphere(const Ray& ray, const Sphere& sphere) noexcept -> std::optional<RayHit>
{
    if (const auto distance_res = intersect_sphere(ray, sphere); distance_res)
    {
        const auto distance = *distance_res;
        const auto position = ray.origin + distance * ray.direction;
        const auto normal = normalize_or(position - sphere.center, ray.direction);
        return RayHit{.distance = distance, .position = position, .normal = normal};
    }
    return std::nullopt;
}

auto hit_aabb(const Ray& ray, const Aabb& aabb) noexcept -> std::optional<RayHit>
{
    if (const auto distance_res = intersect_aabb(ray, aabb); distance_res)
    {
        const auto distance = *distance_res;
        const auto box_min = glm::min(aabb.min, aabb.max);
        const auto box_max = glm::max(aabb.min, aabb.max);
        const auto position = ray.origin + distance * ray.direction;
        const auto normal = aabb_normal_at(position, box_min, box_max);
        return RayHit{.distance = distance, .position = position, .normal = normal};
    }
    return std::nullopt;
}

auto hit_obb(const Ray& ray, const Obb& obb) noexcept -> std::optional<RayHit>
{
    const auto inverse_rotation = glm::inverse(obb.rotation);
    const auto origin = inverse_rotation * (ray.origin - obb.center);
    const auto direction = normalize_or(inverse_rotation * ray.direction, ray.direction);
    const Ray local_ray{.origin = origin, .direction = direction};

    const auto e = glm::abs(obb.half_extent);

    if (const auto hit_res = hit_aabb(local_ray, Aabb{.min = -e, .max = e}); hit_res)
    {
        const auto local_hit = *hit_res;
        const auto position = obb.center + obb.rotation * local_hit.position;
        const auto distance = glm::dot(position - ray.origin, ray.direction);
        const auto normal = normalize_or(obb.rotation * local_hit.normal, ray.direction);
        return RayHit{.distance = distance, .position = position, .normal = normal};
    }
    return std::nullopt;
}

auto hit_capsule(const Ray& ray, const Capsule& capsule) noexcept -> std::optional<RayHit>
{
    {  // Expects
        assert(capsule.radius > 0.0f && "Capsule Radius must be positive");
    }
    const auto r = capsule.radius;
    const auto r2 = r * r;
    const auto segment = capsule.b - capsule.a;
    const auto s2 = glm::dot(segment, segment);
    if (s2 <= 1.0e-12f) return hit_sphere(ray, Sphere{.center = capsule.a, .radius = r});

    const auto ray_direction = normalize_or(ray.direction, k_axis_x);
    const auto ray_to_a = ray.origin - capsule.a;
    const auto ray_segment_dot = glm::dot(ray_direction, segment);
    const auto ray_to_a_dot_ray = glm::dot(ray_to_a, ray_direction);
    const auto ray_to_a_dot_segment = glm::dot(ray_to_a, segment);

    const auto denom = s2 - ray_segment_dot * ray_segment_dot;
    const auto nomin = ray_segment_dot * ray_to_a_dot_segment - s2 * ray_to_a_dot_ray;
    auto ray_t = (std::abs(denom) > 1.0e-8f) ? nomin / denom : 0.0f;

    auto segment_t = (ray_segment_dot * ray_t + ray_to_a_dot_segment) / s2;
    segment_t = std::clamp(segment_t, 0.0f, 1.0f);
    ray_t = std::max(0.0f, ray_segment_dot * segment_t - ray_to_a_dot_ray);

    const auto axis_position = capsule.a + segment_t * segment;
    const auto closest_ray_position = ray.origin + ray_t * ray_direction;
    const auto offset = closest_ray_position - axis_position;
    const auto distance_squared = glm::dot(offset, offset);
    if (distance_squared > r2) return std::nullopt;

    const auto entry_offset = std::sqrt(std::max(0.0f, r2 - distance_squared));
    const auto entry_t = std::max(0.0f, ray_t - entry_offset);
    const auto hit_position = ray.origin + entry_t * ray_direction;

    const auto unclamped = glm::dot(hit_position - capsule.a, segment) / s2;
    const auto hit_segment_t = std::clamp(unclamped, 0.0f, 1.0f);

    const auto hit_axis_position = capsule.a + hit_segment_t * segment;

    return RayHit{
        .distance = entry_t,
        .position = hit_position,
        .normal = normalize_or(hit_position - hit_axis_position, -ray_direction),
    };
}

}  // namespace dans::geom
