#include <oblo/ecs/systems/system_graph.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>

namespace oblo::ecs
{
    using barrier_builder = system_graph_builder::barrier_builder;

    struct system_graph_builder::barrier_systems
    {
        buffered_array<h32<system>, 4> identity;
        buffered_array<type_id, 8> after;
        buffered_array<type_id, 8> before;
    };

    system_graph_builder::system_graph_builder() = default;

    system_graph_builder::~system_graph_builder() = default;

    barrier_builder system_graph_builder::add_system(const system_descriptor& desc,
        std::span<const type_id> before,
        const std::span<const type_id> after,
        std::span<const type_id> barriers)
    {
        const auto id = m_graph.add_system(desc);

        barrier_builder barrierBuilder;
        barrierBuilder.m_builder = this;
        barrierBuilder.m_typeId = desc.typeId;
        barrierBuilder.m_system = id;

        barrierBuilder.as(desc.typeId);

        for (const auto& t : before)
        {
            barrierBuilder.before(t);
        }

        for (const auto& t : after)
        {
            barrierBuilder.after(t);
        }

        for (const auto& t : barriers)
        {
            barrierBuilder.as(t);
        }

        return barrierBuilder;
    }

    expected<system_graph> system_graph_builder::build()
    {
        for (const auto& [barrierId, systems] : m_barrier)
        {
            for (const auto& afterId : systems.after)
            {
                const auto it = m_barrier.find(afterId);

                if (it == m_barrier.end())
                {
                    return unspecified_error;
                }

                for (const auto self : systems.identity)
                {
                    for (const auto other : it->second.identity)
                    {
                        m_graph.add_edge(other, self);
                    }
                }
            }

            for (const auto& beforeId : systems.before)
            {
                const auto it = m_barrier.find(beforeId);

                if (it == m_barrier.end())
                {
                    return unspecified_error;
                }

                for (const auto self : systems.identity)
                {
                    for (const auto other : it->second.identity)
                    {
                        m_graph.add_edge(self, other);
                    }
                }
            }
        }

        return std::move(m_graph);
    }

    system_graph_builder::barrier_systems& system_graph_builder::get_or_add(const type_id& typeId)
    {
        return m_barrier[typeId];
    }

    const barrier_builder& barrier_builder::after(const type_id& type) const
    {
        auto& b = m_builder->get_or_add(m_typeId);
        b.after.push_back(type);
        return *this;
    }

    const barrier_builder& barrier_builder::before(const type_id& type) const
    {
        auto& b = m_builder->get_or_add(m_typeId);
        b.before.push_back(type);
        return *this;
    }

    const barrier_builder& barrier_builder::as(const type_id& type) const
    {
        auto& b = m_builder->get_or_add(type);
        b.identity.push_back(m_system);
        return *this;
    }
}