#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    template <typename Graph>
    [[nodiscard]] bool topological_sort(Graph& g,
        dynamic_array<typename Graph::vertex_handle>& outVertices,
        allocator* temporaryAllocator = get_global_allocator())
    {
        enum class visit_state : u8
        {
            unvisited,
            visiting,
            visited
        };

        const std::span vertices = g.get_vertices();
        const usize numNodes = vertices.size();

        dynamic_array<visit_state> nodeStates{temporaryAllocator};
        nodeStates.resize(numNodes, visit_state::unvisited);

        outVertices.reserve(outVertices.size() + vertices.size());

        // Recursive function for topological sort based on DFS visit
        const auto dfs = [&g, &vertices, &nodeStates, &outVertices](auto&& recurse, usize nodeIndex)
        {
            auto& nodeState = nodeStates[nodeIndex];

            if (nodeState == visit_state::visited)
            {
                return true;
            }

            if (nodeState == visit_state::visiting)
            {
                return false;
            }

            const auto vertex = vertices[nodeIndex];

            nodeState = visit_state::visiting;

            for (const auto outEdge : g.get_out_edges(vertex))
            {
                const auto dst = outEdge.vertex;
                const auto dstIndex = g.get_dense_index(dst);

                if (!recurse(recurse, dstIndex))
                {
                    return false;
                }
            }

            outVertices.emplace_back(vertex);
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
}