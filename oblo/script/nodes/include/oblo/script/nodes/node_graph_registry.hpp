#pragma once

#include <oblo/core/uuid.hpp>

#include <unordered_map>

namespace oblo::script
{
    struct node_descriptor;

    class node_graph_registry
    {
    public:
        bool register_node(const node_descriptor& desc);

        const node_descriptor* find_node(const uuid& id) const;

    private:
        std::unordered_map<uuid, node_descriptor> m_descriptors;
    };
}