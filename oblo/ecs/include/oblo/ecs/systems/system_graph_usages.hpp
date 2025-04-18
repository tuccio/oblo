#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/string/hashed_string_view.hpp>

#include <initializer_list>

namespace oblo::ecs
{
    using system_graph_usage = hashed_string_view;

    class system_graph_usages
    {
    public:
        system_graph_usages() = default;
        system_graph_usages(const system_graph_usages&) = default;
        system_graph_usages(system_graph_usages&&) noexcept = default;

        system_graph_usages(std::initializer_list<system_graph_usage> usages)
        {
            m_usages.append(usages.begin(), usages.end());
        }

        system_graph_usages& operator=(const system_graph_usages&) = default;
        system_graph_usages& operator=(system_graph_usages&&) noexcept = default;

        bool contains(const system_graph_usage& usage) const
        {
            for (auto& u : m_usages)
            {
                if (u == usage)
                {
                    return true;
                }
            }

            return false;
        }

    private:
        buffered_array<system_graph_usage, 8> m_usages;
    };
}