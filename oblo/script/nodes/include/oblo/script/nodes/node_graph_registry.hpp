#pragma once

#include <oblo/core/uuid.hpp>

#include <unordered_map>

namespace oblo::script
{
    struct node_descriptor;

    class node_graph_registry
    {
    public:
        node_graph_registry();
        node_graph_registry(const node_graph_registry&) = delete;
        node_graph_registry(node_graph_registry&&) noexcept = delete;
        ~node_graph_registry();

        node_graph_registry& operator=(const node_graph_registry&) = delete;
        node_graph_registry& operator=(node_graph_registry&&) noexcept = delete;

        bool register_node(const node_descriptor& desc);

        const node_descriptor* find_node(const uuid& id) const;

    private:
        std::unordered_map<uuid, node_descriptor, hash<uuid>> m_descriptors;
    };
}