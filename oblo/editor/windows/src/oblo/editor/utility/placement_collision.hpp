#pragma once

#include <oblo/acceleration/aabb_container.hpp>
#include <oblo/acceleration/bvh.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/math/aabb.hpp>
#include <oblo/math/ray.hpp>
#include <oblo/math/vec3.hpp>

#include <span>

namespace oblo
{
    class resource_registry;
    struct mat4;
}

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo::editor
{
    class placement_collision
    {
    public:
        void build(const resource_registry& resources,
            ecs::entity_registry& reg,
            std::span<const ecs::entity> excludedEntities);

        bool empty() const;

        aabb resolve(const aabb& box) const;

        bool raycast(const ray& r, vec3& outPoint) const;

    private:
        aabb_container m_primitives;
        bvh m_bvh;
    };

    aabb transform_affine(const aabb& local, const mat4& transform);

    void collect_entity_and_descendants(
        const ecs::entity_registry& reg, ecs::entity e, dynamic_array<ecs::entity>& out);
}