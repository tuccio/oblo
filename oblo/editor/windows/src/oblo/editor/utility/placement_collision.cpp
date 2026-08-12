#include <oblo/editor/utility/placement_collision.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/graphics/components/mesh_component.hpp>
#include <oblo/math/mat4.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/components/children_component.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/resources/mesh.hpp>

#include <algorithm>
#include <limits>

namespace oblo::editor
{
    void placement_collision::build(
        const resource_registry& resources, ecs::entity_registry& reg, std::span<const ecs::entity> excludedEntities)
    {
        dynamic_array<aabb> boxes;
        boxes.reserve(64);

        for (auto&& chunk : reg.range<mesh_component, global_transform_component>())
        {
            const auto entities = chunk.get<ecs::entity>();
            const auto meshes = chunk.get<mesh_component>();
            const auto transforms = chunk.get<global_transform_component>();

            for (usize i = 0; i < entities.size(); ++i)
            {
                if (std::find(excludedEntities.begin(), excludedEntities.end(), entities[i]) != excludedEntities.end())
                {
                    continue;
                }

                auto meshRes = resources.get_resource(meshes[i].mesh);

                if (!meshRes)
                {
                    continue;
                }

                meshRes.load_sync();

                if (!meshRes.is_successfully_loaded())
                {
                    continue;
                }

                const aabb local = meshRes->get_aabb();

                if (!is_valid(local))
                {
                    continue;
                }

                boxes.push_back(transform_affine(local, transforms[i].localToWorld));
            }
        }

        m_primitives.clear();
        m_primitives.add(boxes, 0);
        m_bvh.build(m_primitives);
    }

    bool placement_collision::empty() const
    {
        return m_bvh.empty();
    }

    aabb placement_collision::resolve(const aabb& box) const
    {
        if (empty())
        {
            return box;
        }

        constexpr i32 MaxIterations = 8;
        constexpr f32 Epsilon = 1e-3f;

        aabb current = box;

        // Resolve the shallowest overlap one at a time and re-query the broadphase after every move, so corners and
        // stacks of boxes converge to a flush arrangement.
        for (i32 iteration = 0; iteration < MaxIterations; ++iteration)
        {
            f32 bestDistance = std::numeric_limits<f32>::max();
            u32 bestAxis{};
            f32 bestSign{};
            bool found = false;

            const std::span<const aabb> aabbs = m_primitives.get_aabbs();

            m_bvh.intersect_aabb(current,
                [&aabbs, &current, &bestDistance, &bestAxis, &bestSign, &found](u32 offset, u32 numPrimitives)
                {
                    for (u32 i = offset; i < offset + numPrimitives; ++i)
                    {
                        const aabb& other = aabbs[i];

                        bool overlapping = true;

                        for (u32 axis = 0; axis < 3; ++axis)
                        {
                            const f32 depth =
                                min(current.max[axis], other.max[axis]) - max(current.min[axis], other.min[axis]);

                            if (depth <= Epsilon)
                            {
                                overlapping = false;
                                break;
                            }
                        }

                        if (!overlapping)
                        {
                            continue;
                        }

                        for (u32 axis = 0; axis < 3; ++axis)
                        {
                            // Distances to exit the obstacle on either side along this axis. The closest face wins.
                            const f32 distNeg = current.max[axis] - other.min[axis];
                            const f32 distPos = other.max[axis] - current.min[axis];

                            if (distPos < distNeg)
                            {
                                if (distPos < bestDistance)
                                {
                                    bestDistance = distPos;
                                    bestAxis = axis;
                                    bestSign = 1.f;
                                    found = true;
                                }
                            }
                            else if (distNeg < bestDistance)
                            {
                                bestDistance = distNeg;
                                bestAxis = axis;
                                bestSign = -1.f;
                                found = true;
                            }
                        }
                    }
                });

            if (!found)
            {
                break;
            }

            const vec3 correction{bestSign * bestDistance * (bestAxis == 0),
                bestSign * bestDistance * (bestAxis == 1),
                bestSign * bestDistance * (bestAxis == 2)};

            current.min = current.min + correction;
            current.max = current.max + correction;
        }

        return current;
    }

    bool placement_collision::raycast(const ray& r, vec3& outPoint) const
    {
        if (empty())
        {
            return false;
        }

        f32 bestDistance = std::numeric_limits<f32>::max();
        bool hit = false;

        m_bvh.traverse(r,
            [&](u32 offset, u32 numPrimitives, f32)
            {
                aabb_container::hit_result result;

                if (m_primitives.intersect(r, offset, numPrimitives, bestDistance, result))
                {
                    hit = true;
                }
            });

        if (hit)
        {
            outPoint = r.origin + r.direction * bestDistance;
        }

        return hit;
    }

    aabb transform_affine(const aabb& local, const mat4& transform)
    {
        aabb result = aabb::make_invalid();

        for (u32 i = 0; i < 8; ++i)
        {
            const vec3 corner = {
                (i & 1) ? local.max.x : local.min.x,
                (i & 2) ? local.max.y : local.min.y,
                (i & 4) ? local.max.z : local.min.z,
            };

            const vec4 p = transform * vec4{corner.x, corner.y, corner.z, 1.f};
            const vec3 point = {p.x, p.y, p.z};

            result = extend(result, aabb{point, point});
        }

        return result;
    }

    void collect_entity_and_descendants(const ecs::entity_registry& reg, ecs::entity e, dynamic_array<ecs::entity>& out)
    {
        out.push_back(e);

        if (const children_component* const children = reg.try_get<children_component>(e))
        {
            for (const ecs::entity child : children->children)
            {
                if (child && reg.contains(child))
                {
                    collect_entity_and_descendants(reg, child, out);
                }
            }
        }
    }
}