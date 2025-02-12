#include <oblo/core/service_registry_builder.hpp>

#include <oblo/core/graph/directed_graph.hpp>
#include <oblo/core/graph/topological_sort.hpp>
#include <oblo/core/iterator/reverse_range.hpp>

#include <unordered_map>

namespace oblo
{
    expected<success_tag, service_build_error> service_registry_builder::build(service_registry& serviceRegistry)
    {
        using graph_t = directed_graph<usize>;
        graph_t graph;

        dynamic_array<graph_t::vertex_handle> vertices;
        vertices.reserve(m_builders.size());

        std::unordered_map<type_id, graph_t::vertex_handle> typeToBuilderMap;

        for (usize builderIndex = 0; builderIndex < m_builders.size(); ++builderIndex)
        {
            const std::span builtTypes = m_builders[builderIndex].getBases();
            auto v = graph.add_vertex(builderIndex);
            vertices.push_back(v);

            for (const auto& t : builtTypes)
            {
                const auto [it, inserted] = typeToBuilderMap.emplace(t, v);

                if (!inserted && it->second != v)
                {
                    // Seems like the same service is being registered twice
                    return service_build_error::conflict;
                }
            }
        }

        for (usize builderIndex = 0; builderIndex < m_builders.size(); ++builderIndex)
        {
            const std::span requiredTypes = m_builders[builderIndex].getRequires();

            for (const auto& t : requiredTypes)
            {
                const auto it = typeToBuilderMap.find(t);

                if (it != typeToBuilderMap.end())
                {
                    graph.add_edge(it->second, vertices[builderIndex]);
                }
                else if (!serviceRegistry.m_map.contains(t))
                {
                    return service_build_error::missing_dependency;
                }
            }
        }

        vertices.clear();

        if (!topological_sort(graph, vertices))
        {
            return service_build_error::circular_dependency;
        }

        for (const auto& v : reverse_range(vertices))
        {
            const usize builderIndex = graph[v];
            m_builders[builderIndex].build(serviceRegistry);
        }

        return no_error;
    }

    service_registry::~service_registry()
    {
        for (auto [ptr, destroy] : reverse_range(m_services))
        {
            if (destroy && ptr)
            {
                destroy(ptr);
            }
        }
    }
}