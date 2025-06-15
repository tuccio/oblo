#include <oblo/script/nodes/node_graph.hpp>

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/handle_flat_pool_set.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/variant.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/script/nodes/node_descriptor.hpp>
#include <oblo/script/nodes/node_graph_registry.hpp>

namespace oblo::script
{
    namespace
    {
        using node_graph_vertex_handle = node_graph::graph_type::vertex_handle;

        enum class node_flag : u8
        {
            modified,
            enum_max,
        };

        struct node_data
        {
            // Node type id, required for serialization.
            uuid typeId{};

            // Position, required for visualization purposes, should be serialized too.
            vec2 uiPosition{};

            // The node itself.
            unique_ptr<node_interface> node;

            dynamic_array<node_graph_vertex_handle> inputPins;
            dynamic_array<node_graph_vertex_handle> outputPins;

            flags<node_flag> flags{};
        };

        struct pin_data
        {
            // The currently deduced type for the pin.
            uuid deducedType{};

            // User-readable name, only required for visualization purposes.
            string name;

            node_graph_vertex_handle ownerNode{};
        };

        constexpr node_graph_vertex_handle to_vertex_handle(h32<node_graph_node> h)
        {
            return node_graph_vertex_handle{h.value};
        }

        constexpr node_graph_vertex_handle to_vertex_handle(h32<node_graph_in_pin> h)
        {
            return node_graph_vertex_handle{h.value};
        }

        constexpr node_graph_vertex_handle to_vertex_handle(h32<node_graph_out_pin> h)
        {
            return node_graph_vertex_handle{h.value};
        }

        constexpr h32<node_graph_node> to_node_handle(node_graph_vertex_handle h)
        {
            return h32<node_graph_node>{h.value};
        }

        constexpr h32<node_graph_in_pin> to_in_pin_handle(node_graph_vertex_handle h)
        {
            return h32<node_graph_in_pin>{h.value};
        }

        constexpr h32<node_graph_out_pin> to_out_pin_handle(node_graph_vertex_handle h)
        {
            return h32<node_graph_out_pin>{h.value};
        }
    }

    struct node_graph::vertex_type
    {
        variant<node_data, pin_data> data;
    };

    struct node_graph::edge_type
    {
    };

    namespace
    {
        void fetch_input_pins(const node_graph::graph_type& graph,
            node_graph_vertex_handle nodeHandle,
            dynamic_array<h32<node_graph_in_pin>>& pins)
        {
            const auto& nodeVertex = graph.get(nodeHandle);
            const node_data& nodeData = nodeVertex.data.as<node_data>();

            pins.reserve_exponential(pins.size() + nodeData.inputPins.size());

            for (const h32 pin : nodeData.inputPins)
            {
                pins.emplace_back(to_in_pin_handle(pin));
            }
        }

        void fetch_output_pins(const node_graph::graph_type& graph,
            node_graph_vertex_handle nodeHandle,
            dynamic_array<h32<node_graph_out_pin>>& pins)
        {
            auto& nodeVertex = graph.get(nodeHandle);
            const node_data& nodeData = nodeVertex.data.as<node_data>();

            pins.reserve_exponential(pins.size() + nodeData.outputPins.size());

            for (const h32 pin : nodeData.outputPins)
            {
                pins.emplace_back(to_out_pin_handle(pin));
            }
        }
    }

    node_graph::node_graph() = default;

    node_graph::~node_graph() = default;

    void node_graph::init(const node_graph_registry& registry)
    {
        m_registry = &registry;
        m_graph = {};
    }

    h32<node_graph_node> node_graph::add_node(const uuid& id)
    {
        const auto* const desc = m_registry->find_node(id);

        if (!desc)
        {
            return {};
        }

        unique_ptr nodeInstance = desc->instantiate();
        auto* const nodePtr = nodeInstance.get();

        const auto nodeVertex = m_graph.add_vertex(vertex_type{
            .data =
                node_data{
                    .node = std::move(nodeInstance),
                },
        });

        const node_graph_context ctx{*this, nodeVertex};
        nodePtr->on_create(ctx);

        return to_node_handle(nodeVertex);
    }

    void node_graph::remove_node(h32<node_graph_node> nodeHandle)
    {
        // Find node
        // Find all pins
        // Remove pins
        // Call on change on all nodes connected to pins
        (void) nodeHandle;
    }

    void node_graph::fetch_in_pins(h32<node_graph_node> nodeHandle, dynamic_array<h32<node_graph_in_pin>>& pins) const
    {
        fetch_input_pins(m_graph, to_vertex_handle(nodeHandle), pins);
    }

    void node_graph::fetch_out_pins(h32<node_graph_node> nodeHandle, dynamic_array<h32<node_graph_out_pin>>& pins) const
    {
        fetch_output_pins(m_graph, to_vertex_handle(nodeHandle), pins);
    }

    bool node_graph::connect(h32<node_graph_out_pin> src, h32<node_graph_in_pin> dst)
    {
        const h32 srcPinVertex = to_vertex_handle(src);
        const h32 dstPinVertex = to_vertex_handle(dst);

        m_graph.add_edge(srcPinVertex, dstPinVertex);

        const pin_data& pinData = m_graph[dstPinVertex].data.as<pin_data>();

        buffered_array<h32<node_graph_out_pin>, 16> outPinsBuffer;

        dynamic_array<node_graph_vertex_handle> stack;
        stack.reserve(256);
        stack.emplace_back(pinData.ownerNode);

        while (!stack.empty())
        {
            const h32 currentNodeVertex = stack.back();
            stack.pop_back();

            const node_data& currentNodeData = m_graph[currentNodeVertex].data.as<node_data>();

            currentNodeData.node->on_change({*this, currentNodeVertex});

            outPinsBuffer.clear();
            fetch_out_pins(to_node_handle(currentNodeVertex), outPinsBuffer);

            for (const h32 outPinHandle : outPinsBuffer)
            {
                const h32 outPinVertexHandle = to_vertex_handle(outPinHandle);
                const pin_data& outPinData = m_graph[outPinVertexHandle].data.as<pin_data>();

                auto& nextNode = m_graph[outPinData.ownerNode].data.as<node_data>();

                if (nextNode.flags.contains(node_flag::modified))
                {
                    nextNode.flags.unset(node_flag::modified);
                    stack.emplace_back(outPinData.ownerNode);
                }
            }
        }

        return true;
    }

    node_graph_context::node_graph_context(node_graph& g, node_graph_vertex_handle node) :
        m_graph{&g.m_graph}, m_node{node}
    {
    }

    h32<node_graph_in_pin> node_graph_context::add_in_pin(const pin_descriptor& desc) const
    {
        const auto pinVertex = m_graph->add_vertex(node_graph::vertex_type{
            .data =
                pin_data{
                    .name = desc.name,
                    .ownerNode = m_node,
                },
        });

        auto& nodeVertex = m_graph->get(m_node);
        node_data& nodeData = nodeVertex.data.as<node_data>();
        nodeData.inputPins.emplace_back(pinVertex);

        // Add edge from input pin to vertex to ensure topological order
        m_graph->add_edge(pinVertex, m_node);

        return to_in_pin_handle(pinVertex);
    }

    h32<node_graph_out_pin> node_graph_context::add_out_pin(const pin_descriptor& desc) const
    {
        const auto pinVertex = m_graph->add_vertex(node_graph::vertex_type{
            .data =
                pin_data{
                    .name = desc.name,
                    .ownerNode = m_node,
                },
        });

        auto& nodeVertex = m_graph->get(m_node);
        node_data& nodeData = nodeVertex.data.as<node_data>();
        nodeData.outputPins.emplace_back(pinVertex);

        // Add edge from vertex to output pin to ensure topological order
        m_graph->add_edge(m_node, pinVertex);

        return to_out_pin_handle(pinVertex);
    }

    void node_graph_context::fetch_in_pins(dynamic_array<h32<node_graph_in_pin>>& pins) const
    {
        fetch_input_pins(*m_graph, m_node, pins);
    }

    void node_graph_context::fetch_out_pins(dynamic_array<h32<node_graph_out_pin>>& pins) const
    {
        fetch_output_pins(*m_graph, m_node, pins);
    }

    void node_graph_context::mark_modified(h32<node_graph_out_pin> h) const
    {
        for (const auto e : m_graph->get_out_edges(to_vertex_handle(h)))
        {
            const auto dstPinVertex = e.vertex;
            const pin_data& dstPinData = m_graph->get(dstPinVertex).data.as<pin_data>();
            node_data& dstNodeData = m_graph->get(dstPinData.ownerNode).data.as<node_data>();

            dstNodeData.flags.set(node_flag::modified);
        }
    }
}