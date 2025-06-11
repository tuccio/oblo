#include <oblo/script/nodes/node_graph_registry.hpp>

#include <oblo/script/nodes/node_descriptor.hpp>

namespace oblo::script
{
    node_graph_registry::node_graph_registry() = default;

    node_graph_registry::~node_graph_registry() = default;

    bool node_graph_registry::register_node(const node_descriptor& desc)
    {
        const auto [it, inserted] = m_descriptors.emplace(desc.id, desc);
        return inserted;
    }

    const node_descriptor* node_graph_registry::find_node(const uuid& id) const
    {
        const auto it = m_descriptors.find(id);
        return it == m_descriptors.end() ? nullptr : &it->second;
    }
}
