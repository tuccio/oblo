#pragma once

#include <oblo/core/forward.hpp>
#include <oblo/core/uuid.hpp>

#include <unordered_map>

namespace oblo
{
    struct node_descriptor;
    struct node_primitive_type;

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
        bool register_primitive_type(const node_primitive_type& desc);

        const node_descriptor* find_node(const uuid& id) const;
        const node_primitive_type* find_primitive_type(const uuid& id) const;

        void fetch_nodes(dynamic_array<const node_descriptor*>& outNodes) const;
        void fetch_primitive_types(dynamic_array<const node_primitive_type*>& outPrimitiveTypes) const;

    private:
        std::unordered_map<uuid, node_descriptor, hash<uuid>> m_descriptors;
        std::unordered_map<uuid, node_primitive_type, hash<uuid>> m_primitiveTypes;
    };
}