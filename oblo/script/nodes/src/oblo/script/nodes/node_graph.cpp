#include <oblo/script/nodes/node_graph.hpp>

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/variant.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/script/nodes/node_descriptor.hpp>
#include <oblo/script/nodes/node_graph_registry.hpp>

namespace oblo::script
{
    namespace
    {
        struct node_data
        {
            // Node type id, required for serialization.
            uuid typeId{};

            // Position, required for visualization purposes, should be serialized too.
            vec2 uiPosition{};

            // The node itself.
            unique_ptr<node_interface> node;

            dynamic_array<node_graph::graph_type::vertex_handle> inputPins;
            dynamic_array<node_graph::graph_type::vertex_handle> outputPins;
        };

        struct pin_data
        {
            // The currently deduced type for the pin.
            uuid deducedType{};

            // User-readable name, only required for visualization purposes.
            string name;

            node_graph::graph_type::vertex_handle ownerNode{};
        };

        constexpr node_graph::graph_type::vertex_handle to_vertex_handle(h32<node_graph_node> h)
        {
            return node_graph::graph_type::vertex_handle{h.value};
        }

        constexpr node_graph::graph_type::vertex_handle to_vertex_handle(h32<node_graph_in_pin> h)
        {
            return node_graph::graph_type::vertex_handle{h.value};
        }

        constexpr node_graph::graph_type::vertex_handle to_vertex_handle(h32<node_graph_out_pin> h)
        {
            return node_graph::graph_type::vertex_handle{h.value};
        }

        constexpr h32<node_graph_node> to_node_handle(node_graph::graph_type::vertex_handle h)
        {
            return h32<node_graph_node>{h.value};
        }

        constexpr h32<node_graph_in_pin> to_in_pin_handle(node_graph::graph_type::vertex_handle h)
        {
            return h32<node_graph_in_pin>{h.value};
        }

        constexpr h32<node_graph_out_pin> to_out_pin_handle(node_graph::graph_type::vertex_handle h)
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
        /// @brief Only used when connecting to array input pins, to determine to which index the edge is attached.
        u32 arrayIndex;
    };

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

    void node_graph::fetch_in_pins(h32<node_graph_node> nodeHandle, dynamic_array<h32<node_graph_in_pin>>& pins)
    {
        auto& nodeVertex = m_graph.get(to_vertex_handle(nodeHandle));
        node_data& nodeData = nodeVertex.data.as<node_data>();

        pins.reserve_exponential(pins.size() + nodeData.inputPins.size());

        for (const h32 pin : nodeData.inputPins)
        {
            pins.emplace_back(to_in_pin_handle(pin));
        }
    }

    void node_graph::fetch_out_pins(h32<node_graph_node> nodeHandle, dynamic_array<h32<node_graph_out_pin>>& pins)
    {
        auto& nodeVertex = m_graph.get(to_vertex_handle(nodeHandle));
        node_data& nodeData = nodeVertex.data.as<node_data>();

        pins.reserve_exponential(pins.size() + nodeData.outputPins.size());

        for (const h32 pin : nodeData.outputPins)
        {
            pins.emplace_back(to_out_pin_handle(pin));
        }
    }

    bool node_graph::connect(h32<node_graph_out_pin> src, h32<node_graph_in_pin> dst)
    {
        const h32 srcPinVertex = to_vertex_handle(src);
        const h32 dstPinVertex = to_vertex_handle(dst);

        m_graph.add_edge(srcPinVertex, dstPinVertex);

        // TODO: Notify change on destination

        return true;
    }

    node_graph_context::node_graph_context(node_graph& g, node_graph::graph_type::vertex_handle node) :
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
}