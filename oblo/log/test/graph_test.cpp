#include <oblo/core/graph/directed_graph.hpp>
#include <oblo/core/graph/topological_sort.hpp>
#include <oblo/core/string/string_view.hpp>

#include <gtest/gtest.h>

namespace oblo
{
    TEST(directed_graph, directed_graph_basic)
    {
        directed_graph<u32, string_view> graph;

        const auto v0 = graph.add_vertex(666u);
        const auto v1 = graph.add_vertex(1337u);
        const auto v2 = graph.add_vertex(42u);

        ASSERT_TRUE(v0);
        ASSERT_TRUE(v1);
        ASSERT_TRUE(v2);

        const auto e20 = graph.add_edge(v2, v0, "e20");
        const auto e01 = graph.add_edge(v0, v1, "e01");

        ASSERT_EQ(graph[e20], "e20");
        ASSERT_EQ(graph[e01], "e01");

        ASSERT_TRUE(e20);
        ASSERT_TRUE(e01);

        ASSERT_EQ(graph.get_vertex_count(), 3);
        ASSERT_EQ(graph.get_edge_count(), 2);

        dynamic_array<decltype(graph)::vertex_handle> sorted;
        ASSERT_TRUE(topological_sort(graph, sorted));

        ASSERT_EQ(graph.get_vertex_count(), sorted.size());

        const auto expected = {v1, v0, v2};
        ASSERT_EQ(sorted, expected);

        ASSERT_EQ(graph[sorted[0]], 1337u);
        ASSERT_EQ(graph[sorted[1]], 666u);
        ASSERT_EQ(graph[sorted[2]], 42u);
    }
}