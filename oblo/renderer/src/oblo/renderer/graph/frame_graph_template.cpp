#include <oblo/renderer/graph/frame_graph_template.hpp>

namespace oblo
{
    void frame_graph_template::init(const frame_graph_registry& registry)
    {
        m_registry = &registry;
    }

    void frame_graph_template::make_input(vertex_handle pin, string_view name)
    {
        auto& v = m_graph[pin];
        OBLO_ASSERT(v.kind == frame_graph_vertex_kind::pin);

        m_inputs.emplace_back(pin);
        OBLO_ASSERT(v.name.empty() || v.name == name);

        v.name = name;
    }

    void frame_graph_template::make_output(vertex_handle pin, string_view name)
    {
        auto& v = m_graph[pin];
        OBLO_ASSERT(v.kind == frame_graph_vertex_kind::pin);

        m_outputs.emplace_back(pin);
        OBLO_ASSERT(v.name.empty() || v.name == name);

        v.name = name;
    }

    frame_graph_template::vertex_handle frame_graph_template::add_node(const uuid& id,
        const frame_graph_node_desc& desc)
    {
        const auto v = m_graph.add_vertex();

        m_graph[v] = {
            .kind = frame_graph_vertex_kind::node,
            .nodeId = id,
            .nodeDesc = desc,
        };

        for (const auto& pin : desc.pins)
        {
            const auto p = m_graph.add_vertex();

            m_graph[p] = {
                .kind = frame_graph_vertex_kind::pin,
                .pinMemberOffset = pin.offset,
                .nodeId = id,
                .nodeHandle = v,
                .pinDesc = pin.typeDesc,
                .clearDataSink = pin.clearSink,
            };

            m_graph.add_edge(v, p);
        }

        return v;
    }

    bool frame_graph_template::connect(vertex_handle srcPin, vertex_handle dstPin)
    {
        auto& src = m_graph[srcPin];
        auto& dst = m_graph[dstPin];

        OBLO_ASSERT(src.kind == frame_graph_vertex_kind::pin);
        OBLO_ASSERT(dst.kind == frame_graph_vertex_kind::pin);

        m_graph.add_edge(srcPin, dstPin);

        if (!m_graph.has_edge(src.nodeHandle, dst.nodeHandle))
        {
            // We add an extra node between the nodes to ensure execution order
            m_graph.add_edge(src.nodeHandle, dst.nodeHandle);
        }

        return true;
    }

    const frame_graph_template::topology& frame_graph_template::get_graph() const
    {
        return m_graph;
    }

    std::span<const frame_graph_template::vertex_handle> frame_graph_template::get_inputs() const
    {
        return m_inputs;
    }

    std::span<const frame_graph_template::vertex_handle> frame_graph_template::get_outputs() const
    {
        return m_outputs;
    }

    string_view frame_graph_template::get_name(vertex_handle inputOrOutput) const
    {
        return m_graph[inputOrOutput].name;
    }

    frame_graph_template::vertex_handle frame_graph_template::find_pin(vertex_handle node, u32 offset) const
    {
        OBLO_ASSERT(m_graph[node].kind == frame_graph_vertex_kind::node);

        for (const auto e : m_graph.get_out_edges(node))
        {
            const auto& v = m_graph[e.vertex];

            OBLO_ASSERT(v.kind == frame_graph_vertex_kind::pin);
            OBLO_ASSERT(v.nodeId == m_graph[node].nodeId);

            if (v.pinMemberOffset == offset)
            {
                return e.vertex;
            }
        }

        return {};
    }
}