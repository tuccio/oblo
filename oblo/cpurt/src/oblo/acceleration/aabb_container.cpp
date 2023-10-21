#include <oblo/acceleration/aabb_container.hpp>

#include <oblo/core/utility.hpp>
#include <oblo/core/zip_iterator.hpp>
#include <oblo/math/aabb.hpp>
#include <oblo/math/ray_intersection.hpp>
#include <oblo/math/vec3.hpp>

#include <algorithm>

namespace oblo
{
    aabb_container::aabb_container() = default;
    aabb_container::aabb_container(const aabb_container&) = default;
    aabb_container::aabb_container(aabb_container&&) noexcept = default;

    aabb_container::~aabb_container() = default;

    aabb_container& aabb_container::operator=(const aabb_container&) = default;
    aabb_container& aabb_container::operator=(aabb_container&&) noexcept = default;

    u32 aabb_container::size() const
    {
        OBLO_ASSERT(m_ids.size() == m_aabbs.size() && m_centroids.size() == m_aabbs.size());
        return m_aabbs.size();
    }

    void aabb_container::reserve(u32 capacity)
    {
        m_ids.reserve(capacity);
        m_aabbs.reserve(capacity);
        m_centroids.reserve(capacity);
    }

    bool aabb_container::empty() const
    {
        OBLO_ASSERT(m_ids.size() == m_aabbs.size() && m_centroids.size() == m_aabbs.size());
        return m_aabbs.empty();
    }

    void aabb_container::clear()
    {
        m_ids.clear();
        m_aabbs.clear();
        m_centroids.clear();
    }

    void aabb_container::add(std::span<const aabb> aabbs, u32 startingId)
    {
        const auto currentSize = m_aabbs.size();
        const auto newSize = currentSize + aabbs.size();

        m_aabbs.insert(m_aabbs.end(), aabbs.begin(), aabbs.end());

        m_ids.reserve(newSize);
        m_centroids.reserve(newSize);

        for (const auto& aabb : aabbs)
        {
            const auto centroid = (aabb.max + aabb.min) * .5f;

            m_ids.emplace_back(startingId++);
            m_centroids.emplace_back(centroid);
        }
    }

    aabb aabb_container::primitives_bounds(u32 begin, u32 end) const
    {
        return oblo::compute_aabb(std::span(m_aabbs).subspan(begin, end - begin));
    }

    aabb aabb_container::centroids_bounds(u32 begin, u32 end) const
    {
        return oblo::compute_aabb(std::span(m_centroids).subspan(begin, end - begin));
    }

    u32 aabb_container::partition_by_axis(u32 beginIndex, u32 endIndex, u8 axisIndex, f32 midPoint)
    {
        const auto beginIt = zip_iterator{m_aabbs.begin(), m_aabbs.begin(), m_centroids.begin()};

        const auto rangeBegin = beginIt + beginIndex;
        const auto rangeEnd = beginIt + endIndex;

        const auto midIt = std::partition(rangeBegin,
            rangeEnd,
            [axisIndex, midPoint](const auto& element) { return std::get<2>(element)[axisIndex] < midPoint; });

        return narrow_cast<u32>(midIt - beginIt);
    }

    bool aabb_container::intersect(
        const ray& ray, u32 beginIndex, u16 numPrimitives, f32& distance, hit_result& result) const
    {
        const auto aabbs = std::span{m_aabbs}.subspan(beginIndex, numPrimitives);
        u32 current = beginIndex;

        bool hit = false;

        for (const auto& aabb : aabbs)
        {
            if (f32 t0, t1; oblo::intersect(ray, aabb, distance, t0, t1) && t0 < distance)
            {
                result.index = current;
                hit = true;
                distance = t0;
            }

            ++current;
        }

        return hit;
    }

    std::span<const u32> aabb_container::get_ids() const
    {
        return m_ids;
    }

    std::span<const aabb> aabb_container::get_aabbs() const
    {
        return m_aabbs;
    }

    std::span<const vec3> aabb_container::get_centroids() const
    {
        return m_centroids;
    }
}