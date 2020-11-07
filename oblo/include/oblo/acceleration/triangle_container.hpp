#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>

#include <span>
#include <vector>

namespace oblo
{
    struct aabb;
    struct triangle;
    struct vec3;

    class triangle_container
    {
    public:
        triangle_container();
        triangle_container(const triangle_container&);
        triangle_container(triangle_container&&) noexcept;
        ~triangle_container();

        triangle_container& operator=(const triangle_container&);
        triangle_container& operator=(triangle_container&&) noexcept;

        u32 size() const;

        bool empty() const
        {
            OBLO_ASSERT(m_aabbs.empty() == m_triangles.empty() && m_centroids.empty() == m_triangles.empty());
            return m_triangles.empty();
        }

        void clear();

        void add(std::span<const triangle> triangles);

        aabb primitives_bounds(u32 begin, u32 end) const;
        aabb centroids_bounds(u32 begin, u32 end) const;

        u32 partition_by_axis(u32 beginIndex, u32 endIndex, u8 axisIndex, f32 midPoint);

        std::span<const triangle> get_triangles() const;

    private:
        std::vector<triangle> m_triangles;
        std::vector<aabb> m_aabbs;
        std::vector<vec3> m_centroids;
    };
}