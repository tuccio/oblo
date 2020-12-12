#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>

#include <span>
#include <vector>

namespace oblo
{
    struct aabb;
    struct ray;
    struct vec3;

    class aabb_container
    {
    public:
        struct hit_result;

        static constexpr hit_result make_invalid_hit_result();

        aabb_container();
        aabb_container(const aabb_container&);
        aabb_container(aabb_container&&) noexcept;
        ~aabb_container();

        aabb_container& operator=(const aabb_container&);
        aabb_container& operator=(aabb_container&&) noexcept;

        u32 size() const;
        void reserve(u32 capacity);

        bool empty() const;

        void clear();

        void add(std::span<const aabb> aabbs, u32 startingId);

        aabb primitives_bounds(u32 begin, u32 end) const;
        aabb centroids_bounds(u32 begin, u32 end) const;

        u32 partition_by_axis(u32 beginIndex, u32 endIndex, u8 axisIndex, f32 midPoint);

        bool intersect(const ray& ray, u32 beginIndex, u16 numPrimitives, f32& distance, hit_result& result) const;

        std::span<const u32> get_ids() const;
        std::span<const aabb> get_aabbs() const;
        std::span<const vec3> get_centroids() const;

    private:
        std::vector<u32> m_ids;
        std::vector<aabb> m_aabbs;
        std::vector<vec3> m_centroids;
    };

    struct aabb_container::hit_result
    {
        u32 index;
    };
}