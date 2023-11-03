#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>
#include <oblo/math/aabb.hpp>
#include <oblo/math/ray_intersection.hpp>

#include <algorithm>
#include <concepts>
#include <memory>
#include <memory_resource>
#include <numeric>
#include <span>

namespace oblo
{
    class bvh
    {
    public:
        bvh() = default;
        bvh(bvh&&) noexcept = default;
        bvh& operator=(bvh&&) noexcept = default;

        ~bvh() = default;

        template <typename PrimitiveContainer>
        void build(PrimitiveContainer& primitives)
        {
            OBLO_ASSERT(primitives.size() <= narrow_cast<u32>(-1));
            clear();

            if (primitives.empty())
            {
                return;
            }

            m_node = std::make_unique<bvh_node>();

            const auto size = narrow_cast<u32>(primitives.size());
            build_impl_sah(*m_node, primitives, 0, size);
        }

        bool empty() const
        {
            return !bool{m_node};
        }

        void clear()
        {
            m_node.reset();
        }

        template <typename F>
        void visit(F&& visitor) const
            requires std::invocable<F, u32, aabb, u32, u32>
        {
            if (m_node)
            {
                visit_impl(*m_node, visitor);
            }
        }

        template <typename F>
        void traverse(const ray& ray, F&& f) const
        {
            constexpr auto BufferSize = 4096;
            constexpr auto MaxStackElements = BufferSize / sizeof(void*);

            std::byte buffer[BufferSize];
            std::pmr::monotonic_buffer_resource resource{buffer, BufferSize};

            std::pmr::vector<const bvh_node*> nodesStack{&resource};
            nodesStack.reserve(MaxStackElements);

            nodesStack.emplace_back(m_node.get());

            f32 distance = std::numeric_limits<f32>::max();

            while (!nodesStack.empty())
            {
                const bvh_node* const node = nodesStack.back();
                nodesStack.pop_back();

                if (float t0, t1; !oblo::intersect(ray, node->bounds, distance, t0, t1))
                {
                    continue;
                }

                if (node->numPrimitives > 0)
                {
                    OBLO_ASSERT(node->children == nullptr);
                    f(node->offset, node->numPrimitives, distance);
                }
                else if (node->children)
                {
                    // TODO: Pick the closest child first
                    const auto children = node->children.get();
                    nodesStack.emplace_back(children);
                    nodesStack.emplace_back(children + 1);
                }
            }
        }

        aabb get_bounds() const
        {
            return m_node ? m_node->bounds : aabb::make_invalid();
        }

    private:
        struct bvh_node
        {
            aabb bounds;
            std::unique_ptr<bvh_node[]> children;
            u32 offset;
            u16 numPrimitives;
            i8 splitAxis;
        };

    private:
        template <typename PrimitiveContainer>
        void build_impl(bvh_node& node, PrimitiveContainer& primitives, u32 begin, u32 end) const
        {
            node.bounds = primitives.primitives_bounds(begin, end);

            if (const auto numPrimitives = end - begin; numPrimitives <= 1)
            {
                init_leaf(node, begin, numPrimitives);
            }
            else
            {
                const auto centroidsBounds = primitives.centroids_bounds(begin, end);
                const auto maxExtentAxis = max_extent(centroidsBounds);

                f32 midPoint = (centroidsBounds.min[maxExtentAxis] + centroidsBounds.max[maxExtentAxis]) / 2;
                u32 midIndex = primitives.partition_by_axis(begin, end, maxExtentAxis, midPoint);

                if (midIndex == begin || midIndex == end)
                {
                    // TODO: Could split using different heuristics
                    init_leaf(node, begin, end - begin);
                }
                else
                {
                    node.children = std::make_unique<bvh_node[]>(2);
                    build_impl(node.children[0], primitives, begin, midIndex);
                    build_impl(node.children[1], primitives, midIndex, end);
                }
            }
        }

        template <typename PrimitiveContainer>
        void build_impl_sah(bvh_node& node, PrimitiveContainer& primitives, u32 begin, u32 end) const
        {
            node.bounds = primitives.primitives_bounds(begin, end);

            if (const auto numPrimitives = end - begin; numPrimitives <= 4)
            {
                init_leaf(node, begin, narrow_cast<u16>(numPrimitives));
            }
            else
            {
                const auto centroidsBounds = primitives.centroids_bounds(begin, end);
                const auto maxExtentAxis = max_extent(centroidsBounds);

                const auto centroids = primitives.get_centroids();
                const auto bounds = primitives.get_aabbs();

                constexpr auto numBuckets = 16;

                struct bucket_data
                {
                    u32 count;
                    aabb bounds;
                };

                constexpr auto init_bucket_data = [] { return bucket_data{0, aabb::make_invalid()}; };

                bucket_data buckets[numBuckets];
                std::fill(std::begin(buckets), std::end(buckets), init_bucket_data());

                const auto maxDistance = centroidsBounds.max[maxExtentAxis] - centroidsBounds.min[maxExtentAxis];
                OBLO_ASSERT(maxDistance > 0.f);

                for (u32 primitiveIndex = begin; primitiveIndex < end; ++primitiveIndex)
                {
                    const auto distance = centroids[primitiveIndex][maxExtentAxis] - centroidsBounds.min[maxExtentAxis];
                    const auto bucketIndex =
                        std::min(narrow_cast<i32>(numBuckets * distance / maxDistance), numBuckets - 1);

                    auto& bucket = buckets[bucketIndex];

                    ++bucket.count;
                    bucket.bounds = extend(bucket.bounds, bounds[primitiveIndex]);
                }

                bucket_data forwardScan[numBuckets];
                bucket_data backwardScan[numBuckets];

                constexpr auto accumulate = [](const bucket_data& lhs, const bucket_data& rhs) {
                    return bucket_data{lhs.count + rhs.count, extend(lhs.bounds, rhs.bounds)};
                };

                std::inclusive_scan(std::begin(buckets),
                    std::end(buckets),
                    std::begin(forwardScan),
                    accumulate,
                    init_bucket_data());

                std::exclusive_scan(std::rbegin(buckets),
                    std::rend(buckets),
                    std::rbegin(backwardScan),
                    init_bucket_data(),
                    accumulate);

                constexpr auto cost = [](const bucket_data& a, const bucket_data& b)
                {
                    constexpr auto half_surface = [](const aabb& bounds)
                    {
                        const auto d = bounds.max - bounds.min;
                        return d.x * d.y + d.x * d.z + d.y * d.z;
                    };

                    return a.count * half_surface(a.bounds) + b.count * half_surface(b.bounds);
                };

                float minCost = cost(forwardScan[0], backwardScan[0]);
                i32 bucketIndex = 0;

                // The last element actually makes no sense
                for (i32 i = 1; i < numBuckets - 1; ++i)
                {
                    const auto currentCost = cost(forwardScan[i], backwardScan[i]);

                    if (currentCost < minCost)
                    {
                        minCost = currentCost;
                        bucketIndex = i;
                    }
                }

                const auto splitPoint = centroidsBounds.min[maxExtentAxis] + bucketIndex * maxDistance / numBuckets;
                u32 midIndex = primitives.partition_by_axis(begin, end, maxExtentAxis, splitPoint);

                if (midIndex == begin || midIndex == end)
                {
                    // TODO: Could split using different heuristics
                    init_leaf(node, begin, narrow_cast<u16>(end - begin));
                }
                else
                {
                    node.splitAxis = narrow_cast<i8>(maxExtentAxis);
                    node.children = std::make_unique<bvh_node[]>(2);
                    build_impl_sah(node.children[0], primitives, begin, midIndex);
                    build_impl_sah(node.children[1], primitives, midIndex, end);
                }
            }
        }

        void init_leaf(bvh_node& node, u32 offset, u16 numPrimitives) const
        {
            OBLO_ASSERT(!node.children);
            node.offset = offset;
            node.numPrimitives = numPrimitives;
            node.splitAxis = -1;
        }

        template <typename F>
        void visit_impl(bvh_node& node, F&& visitor, u32 depth = 0) const
        {
            visitor(depth, node.bounds, node.offset, node.numPrimitives);

            if (node.children)
            {
                visit_impl(node.children[0], visitor, depth + 1);
                visit_impl(node.children[1], visitor, depth + 1);
            }
        }

    private:
        std::unique_ptr<bvh_node> m_node;
    };
}