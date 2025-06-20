#include <oblo/nodes/node_graph_registry.hpp>

#include <oblo/core/dynamic_array.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_primitive_type.hpp>

namespace oblo
{
    node_graph_registry::node_graph_registry() = default;

    node_graph_registry::~node_graph_registry() = default;

    bool node_graph_registry::register_node(const node_descriptor& desc)
    {
        const auto [it, inserted] = m_descriptors.emplace(desc.id, desc);
        return inserted;
    }

    bool node_graph_registry::register_primitive_type(const node_primitive_type& desc)
    {
        const auto [it, inserted] = m_primitiveTypes.emplace(desc.id, desc);
        return inserted;
    }

    const node_descriptor* node_graph_registry::find_node(const uuid& id) const
    {
        const auto it = m_descriptors.find(id);
        return it == m_descriptors.end() ? nullptr : &it->second;
    }

    const node_primitive_type* node_graph_registry::find_primitive_type(const uuid& id) const
    {
        const auto it = m_primitiveTypes.find(id);
        return it == m_primitiveTypes.end() ? nullptr : &it->second;
    }

    void node_graph_registry::fetch_nodes(dynamic_array<const node_descriptor*>& outNodes) const
    {
        outNodes.reserve(outNodes.size() + m_descriptors.size());

        for (const auto& [id, desc] : m_descriptors)
        {
            outNodes.emplace_back(&desc);
        }
    }

    void node_graph_registry::fetch_primitive_types(dynamic_array<const node_primitive_type*>& outPrimitiveTypes) const
    {
        outPrimitiveTypes.reserve(outPrimitiveTypes.size() + m_primitiveTypes.size());

        for (const auto& [id, desc] : m_primitiveTypes)
        {
            outPrimitiveTypes.emplace_back(&desc);
        }
    }
}
