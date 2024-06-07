#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/graph/dfs_visit.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    template <typename Graph>
    [[nodiscard]] bool topological_sort(Graph& g,
        dynamic_array<typename Graph::vertex_handle>& outVertices,
        allocator* temporaryAllocator = get_global_allocator())
    {
        outVertices.reserve(outVertices.size() + g.get_vertex_count());

        return dfs_visit_template<graph_visit_flag::post_visit | graph_visit_flag::fail_if_not_dag>(
            g,
            [&outVertices](Graph::vertex_handle v) { outVertices.push_back(v); },
            temporaryAllocator);
    }
}