#include <oblo/vulkan/graph/frame_graph_template.hpp>

namespace oblo::vk
{
    void frame_graph_template::init(const frame_graph_registry& registry)
    {
        m_registry = &registry;
        m_input = m_graph.add_vertex(frame_graph_vertex_kind::pin);
        m_output = m_graph.add_vertex(frame_graph_vertex_kind::pin);

        m_graph[m_input].name = "<in>";
        m_graph[m_output].name = "<out>";
    }

    void frame_graph_template::make_input(vertex_handle pin, std::string_view name)
    {
        auto& dst = m_graph[pin];
        OBLO_ASSERT(dst.kind == frame_graph_vertex_kind::pin);

        m_graph.add_edge(m_input, pin);
        dst.name = name;
    }

    void frame_graph_template::make_output(vertex_handle pin, std::string_view name)
    {
        auto& src = m_graph[pin];
        OBLO_ASSERT(src.kind == frame_graph_vertex_kind::pin);

        m_graph.add_edge(pin, m_output);
        src.name = name;
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
        m_graph.add_edge(src.nodeHandle, dst.nodeHandle);

        return true;
    }

    const frame_graph_template::topology& frame_graph_template::get_graph() const
    {
        return m_graph;
    }

    std::span<const frame_graph_template::edge_reference> frame_graph_template::get_inputs() const
    {
        return m_graph.get_out_edges(m_input);
    }

    std::span<const frame_graph_template::edge_reference> frame_graph_template::get_outputs() const
    {
        return m_graph.get_in_edges(m_output);
    }

    std::string_view frame_graph_template::get_name(edge_reference inputOrOutput) const
    {
        return m_graph[inputOrOutput.vertex].name;
    }

    frame_graph_template::vertex_handle frame_graph_template::find_pin(vertex_handle node, u32 offset) const
    {
        OBLO_ASSERT(m_graph[node].kind == frame_graph_vertex_kind::node);

        for (const edge_reference e : m_graph.get_out_edges(node))
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