#include <oblo/renderer/graph/frame_graph_registry.hpp>

namespace oblo
{
    bool frame_graph_registry::register_node(const uuid& id, frame_graph_node_desc&& desc)
    {
        const auto [it, ok] = m_nodes.try_emplace(id, std::move(desc));
        return ok;
    }

    void frame_graph_registry::unregister_node(const uuid& id)
    {
        m_nodes.erase(id);
    }

    const frame_graph_node_desc* frame_graph_registry::find_node(const uuid& id) const
    {
        const auto it = m_nodes.find(id);
        return it == m_nodes.end() ? nullptr : &it->second;
    }
}
