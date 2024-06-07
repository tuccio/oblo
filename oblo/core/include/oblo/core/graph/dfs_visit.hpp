#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/types.hpp>

#include <span>

namespace oblo
{
    enum class graph_visit_flag
    {
        fail_if_not_dag,
        pre_visit,
        post_visit,
        enum_max,
    };

    template <flags<graph_visit_flag> Flags, typename Graph, typename Visitor>
    bool dfs_visit_template(Graph& g, Visitor&& v, allocator* temporaryAllocator)
    {
        enum class visit_state : u8
        {
            unvisited,
            visiting,
            visited,
        };

        const std::span vertices = g.get_vertices();
        const usize numNodes = vertices.size();

        dynamic_array<visit_state> nodeStates{temporaryAllocator};
        nodeStates.resize(numNodes, visit_state::unvisited);

        // Recursive function for now
        const auto dfs = [&g, &vertices, &nodeStates, &v](auto&& recurse, usize nodeIndex)
        {
            auto& nodeState = nodeStates[nodeIndex];

            if (nodeState == visit_state::visited)
            {
                return true;
            }

            if (nodeState == visit_state::visiting)
            {
                constexpr bool canContinue = !Flags.contains(graph_visit_flag::fail_if_not_dag);
                return canContinue;
            }

            const auto vertex = vertices[nodeIndex];

            nodeState = visit_state::visiting;

            if constexpr (Flags.contains(graph_visit_flag::pre_visit))
            {
                v(vertex);
            }

            for (const auto outEdge : g.get_out_edges(vertex))
            {
                const auto dst = outEdge.vertex;
                const auto dstIndex = g.get_dense_index(dst);

                if (!recurse(recurse, dstIndex))
                {
                    return false;
                }
            }

            if constexpr (Flags.contains(graph_visit_flag::post_visit))
            {
                v(vertex);
            }

            nodeState = visit_state::visited;

            return true;
        };

        for (usize i = 0u; i < numNodes; ++i)
        {
            if (!dfs(dfs, i))
            {
                return false;
            }
        }

        return true;
    }

    template <typename Graph, typename Visitor>
    bool dfs_visit(Graph& g, Visitor&& v, allocator* temporaryAllocator = get_global_allocator())
    {
        return dfs_visit_template<graph_visit_flag::pre_visit>(g, std::forward<Visitor>(v), temporaryAllocator);
    }
}