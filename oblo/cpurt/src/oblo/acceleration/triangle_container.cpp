#include <oblo/acceleration/triangle_container.hpp>

#include <oblo/core/iterator/zip_iterator.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/aabb.hpp>
#include <oblo/math/ray_intersection.hpp>
#include <oblo/math/triangle.hpp>
#include <oblo/math/vec3.hpp>

#include <algorithm>

namespace oblo
{
    triangle_container::triangle_container() = default;
    triangle_container::triangle_container(const triangle_container&) = default;
    triangle_container::triangle_container(triangle_container&&) noexcept = default;

    triangle_container::~triangle_container() = default;

    triangle_container& triangle_container::operator=(const triangle_container&) = default;
    triangle_container& triangle_container::operator=(triangle_container&&) noexcept = default;

    u32 triangle_container::size() const
    {
        OBLO_ASSERT(m_aabbs.size() == m_triangles.size() && m_centroids.size() == m_triangles.size());
        return u32(m_triangles.size());
    }

    bool triangle_container::empty() const
    {
        OBLO_ASSERT(m_aabbs.size() == m_triangles.size() && m_centroids.size() == m_triangles.size());
        return m_triangles.empty();
    }

    void triangle_container::clear()
    {
        m_triangles.clear();
        m_aabbs.clear();
        m_centroids.clear();
    }

    void triangle_container::add(std::span<const triangle> triangles)
    {
        const auto currentSize = m_triangles.size();
        const auto newSize = currentSize + triangles.size();
        m_triangles.insert(m_triangles.end(), triangles.begin(), triangles.end());

        m_aabbs.reserve(newSize);
        m_centroids.reserve(newSize);
        m_normals.reserve(newSize);

        for (const auto& triangle : triangles)
        {
            const auto aabb = oblo::compute_aabb(std::span{triangle.v});
            const auto centroid = (triangle.v[0] + triangle.v[1] + triangle.v[2]) / 3.f;
            const auto normal = normalize(cross(triangle.v[1] - triangle.v[0], triangle.v[2] - triangle.v[0]));

            m_aabbs.emplace_back(aabb);
            m_centroids.emplace_back(centroid);
            m_normals.emplace_back(normal);
        }
    }

    void triangle_container::reserve(std::size_t numTriangles)
    {
        m_triangles.reserve(numTriangles);
        m_aabbs.reserve(numTriangles);
        m_centroids.reserve(numTriangles);
        m_normals.reserve(numTriangles);
    }

    aabb triangle_container::primitives_bounds(u32 begin, u32 end) const
    {
        return oblo::compute_aabb(std::span(m_aabbs).subspan(begin, end - begin));
    }

    aabb triangle_container::centroids_bounds(u32 begin, u32 end) const
    {
        return oblo::compute_aabb(std::span(m_centroids).subspan(begin, end - begin));
    }

    u32 triangle_container::partition_by_axis(u32 beginIndex, u32 endIndex, u8 axisIndex, f32 midPoint)
    {
        const auto beginIt = zip_iterator{m_triangles.begin(), m_aabbs.begin(), m_centroids.begin(), m_normals.begin()};

        const auto rangeBegin = beginIt + beginIndex;
        const auto rangeEnd = beginIt + endIndex;

        const auto midIt = std::partition(rangeBegin,
            rangeEnd,
            [axisIndex, midPoint](const auto& element) { return std::get<2>(element)[axisIndex] < midPoint; });

        return narrow_cast<u32>(midIt - beginIt);
    }

    bool triangle_container::intersect(
        const ray& ray, u32 beginIndex, u16 numPrimitives, f32& distance, hit_result& result) const
    {
        const auto triangles = std::span{m_triangles}.subspan(beginIndex, numPrimitives);
        u32 current = beginIndex;

        bool hit = false;

        for (const auto& triangle : triangles)
        {
            if (f32 triangleDistance; oblo::intersect(ray, triangle, triangleDistance) && triangleDistance < distance)
            {
                result.index = current;
                hit = true;
                distance = triangleDistance;
            }

            ++current;
        }

        return hit;
    }

    std::span<const triangle> triangle_container::get_triangles() const
    {
        return m_triangles;
    }

    std::span<const aabb> triangle_container::get_aabbs() const
    {
        return m_aabbs;
    }

    std::span<const vec3> triangle_container::get_centroids() const
    {
        return m_centroids;
    }

    std::span<const vec3> triangle_container::get_normals() const
    {
        return m_normals;
    }
}