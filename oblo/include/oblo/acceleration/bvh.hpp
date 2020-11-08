#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>
#include <oblo/math/aabb.hpp>

#include <concepts>
#include <memory>
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

            if (primitives.empty())
            {
                clear();
                return;
            }

            m_node = std::make_unique<bvh_node>();

            const auto size = narrow_cast<u32>(primitives.size());
            build_impl(*m_node, primitives, 0, size);
        }

        void clear()
        {
            m_node.reset();
        }

        template <typename F>
        void visit(F&& visitor) const requires std::invocable<F, u32, aabb, u32, u32>
        {
            if (m_node)
            {
                visit_impl(*m_node, visitor);
            }
        }

    private:
        struct bvh_node
        {
            aabb bounds;
            std::unique_ptr<bvh_node[]> children;
            u32 offset;
            u32 numPrimitives;
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
                    init_leaf(node, begin, end);
                }
                else
                {
                    node.children = std::make_unique<bvh_node[]>(2);
                    build_impl(node.children[0], primitives, begin, midIndex);
                    build_impl(node.children[1], primitives, midIndex + 1, end);
                }
            }
        }

        void init_leaf(bvh_node& node, u32 offset, u32 numPrimitives) const
        {
            OBLO_ASSERT(!node.children);
            node.offset = offset;
            node.numPrimitives = numPrimitives;
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