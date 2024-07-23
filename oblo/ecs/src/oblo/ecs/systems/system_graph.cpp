#include <oblo/ecs/systems/system_graph.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/graph/topological_sort.hpp>
#include <oblo/core/iterator/reverse_range.hpp>
#include <oblo/ecs/systems/system_seq_executor.hpp>

namespace oblo::ecs
{
    using system_graph_vertex = directed_graph<system>::vertex_handle;

    class system
    {
    public:
        system_descriptor descriptor;
    };

    system_graph::system_graph() = default;
    system_graph::system_graph(const system_graph&) = default;
    system_graph::system_graph(system_graph&&) noexcept = default;

    system_graph::~system_graph() = default;

    system_graph& system_graph::operator=(const system_graph&) = default;
    system_graph& system_graph::operator=(system_graph&&) noexcept = default;

    h32<system> system_graph::add_system(const system_descriptor& desc)
    {
        const auto h = m_systems.add_vertex(desc);
        return h32<system>{h.value};
    }

    void system_graph::add_edge(h32<system> from, h32<system> to)
    {
        const auto vFrom = system_graph_vertex{from.value};
        const auto vTo = system_graph_vertex{to.value};

        if (!m_systems.has_edge(vFrom, vTo))
        {
            m_systems.add_edge(vFrom, vTo);
        }
    }

    expected<system_seq_executor> system_graph::instantiate() const
    {
        system_seq_executor executor;
        executor.reserve(m_systems.get_vertex_count());

        dynamic_array<system_graph_vertex> vertices;

        if (!topological_sort(m_systems, vertices))
        {
            return unspecified_error;
        }

        for (const auto v : reverse_range(vertices))
        {
            const auto& system = m_systems[v];

            if (!system.descriptor.update)
            {
                // Barriers have no update, they exist just for ordering
                continue;
            }

            executor.push(system.descriptor);
        }

        return executor;
    }
}