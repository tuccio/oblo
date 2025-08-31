#pragma once

#include <oblo/core/forward.hpp>
#include <oblo/core/pair.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/nodes/node_primitive_type.hpp>

#include <unordered_map>

namespace oblo
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

        bool register_node(node_descriptor desc);
        bool register_primitive_type(node_primitive_type desc);

        const node_descriptor* find_node(const uuid& id) const;
        const node_primitive_type* find_primitive_type(const uuid& id) const;

        void fetch_nodes(dynamic_array<const node_descriptor*>& outNodes) const;
        void fetch_primitive_types(dynamic_array<const node_primitive_type*>& outPrimitiveTypes) const;

        uuid find_promotion_rule(const uuid& lhs, const uuid& rhs) const;

    private:
        template <typename Key, typename Value>
        using unordered_map = std::unordered_map<Key, Value, hash<Key>>;

    private:
        unordered_map<uuid, node_descriptor> m_descriptors;
        unordered_map<uuid, node_primitive_type> m_primitiveTypes;
        uuid m_primitiveKindToTypeId[u32(node_primitive_kind::enum_max)]{};
    };
}