#include <oblo/acceleration/triangle_container.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/core/zip_iterator.hpp>
#include <oblo/math/aabb.hpp>
#include <oblo/math/triangle.hpp>
#include <oblo/math/vec3.hpp>

#include <algorithm>

namespace oblo
{
    triangle_container::triangle_container(const triangle_container&) = default;
    triangle_container::triangle_container(triangle_container&&) noexcept = default;

    triangle_container::~triangle_container() = default;

    triangle_container& triangle_container::operator=(const triangle_container&) = default;
    triangle_container& triangle_container::operator=(triangle_container&&) noexcept = default;

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

        for (const auto& triangle : triangles)
        {
            const auto aabb = oblo::compute_aabb(std::span{triangle.v});
            const auto centroid = (triangle.v[0] + triangle.v[1] + triangle.v[2]) / 3.f;

            m_aabbs.emplace_back(aabb);
            m_centroids.emplace_back(centroid);
        }
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
        const auto beginIt = zip_iterator{m_triangles.begin(), m_aabbs.begin(), m_centroids.begin()};

        const auto rangeBegin = beginIt + beginIndex;
        const auto rangeEnd = beginIt + endIndex;

        const auto midIt = std::partition(rangeBegin,
                                          rangeEnd,
                                          [axisIndex, midPoint](const auto& element)
                                          { return std::get<2>(element)[axisIndex] < midPoint; });

        return narrow_cast<u32>(midIt - beginIt);
    }
}