#include <oblo/ecs/systems/system_graph.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/ecs/systems/system_seq_executor.hpp>

namespace oblo::ecs
{
    struct system_graph::system_data
    {
        system_descriptor descriptor;
        std::vector<h32<system>> edges;
    };

    system_graph::system_graph() = default;
    system_graph::system_graph(const system_graph&) = default;
    system_graph::system_graph(system_graph&&) noexcept = default;

    system_graph::~system_graph() = default;

    system_graph& system_graph::operator=(const system_graph&) = default;
    system_graph& system_graph::operator=(system_graph&&) noexcept = default;

    h32<system> system_graph::add_system(const system_descriptor& desc)
    {
        m_systems.emplace_back(desc);
        return h32<system>{u32(m_systems.size())};
    }

    system_seq_executor system_graph::instantiate() const
    {
        system_seq_executor executor;
        executor.reserve(m_systems.size());

        // TODO: Topological sort
        for (auto& system : m_systems)
        {
            executor.push(system.descriptor);
            OBLO_ASSERT(system.edges.empty(), "Need to implement topological sort");
        }

        return executor;
    }
}