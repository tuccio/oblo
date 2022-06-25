#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>

#include <span>
#include <vector>

namespace oblo
{
    struct aabb;
    struct triangle;
    struct ray;
    struct vec3;

    class triangle_container
    {
    public:
        struct hit_result;

        static constexpr hit_result make_invalid_hit_result();

        triangle_container();
        triangle_container(const triangle_container&);
        triangle_container(triangle_container&&) noexcept;
        ~triangle_container();

        triangle_container& operator=(const triangle_container&);
        triangle_container& operator=(triangle_container&&) noexcept;

        u32 size() const;

        bool empty() const;

        void clear();

        void add(std::span<const triangle> triangles);
        void reserve(std::size_t numTriangles);

        aabb primitives_bounds(u32 begin, u32 end) const;
        aabb centroids_bounds(u32 begin, u32 end) const;

        u32 partition_by_axis(u32 beginIndex, u32 endIndex, u8 axisIndex, f32 midPoint);

        bool intersect(const ray& ray, u32 beginIndex, u16 numPrimitives, f32& distance, hit_result& result) const;

        std::span<const triangle> get_triangles() const;
        std::span<const aabb> get_aabbs() const;
        std::span<const vec3> get_centroids() const;
        std::span<const vec3> get_normals() const;

    private:
        std::vector<triangle> m_triangles;
        std::vector<aabb> m_aabbs;
        std::vector<vec3> m_centroids;
        std::vector<vec3> m_normals;
    };

    struct triangle_container::hit_result
    {
        u32 index;
    };
}