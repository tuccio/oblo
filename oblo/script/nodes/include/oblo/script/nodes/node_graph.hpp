#pragma once

#include <oblo/core/graph/directed_graph.hpp>
#include <oblo/script/nodes/node_instance.hpp>

namespace oblo::script
{
    struct node_graph_node;
    struct node_graph_in_pin;
    struct node_graph_out_pin;

    class node_graph_context;
    class node_graph_registry;

    class node_graph
    {
    public:
        struct edge_type;
        struct vertex_type;

        using graph_type = directed_graph<vertex_type, edge_type>;

    public:
        void init(const node_graph_registry& registry);

        h32<node_graph_node> add_node(const uuid& id);
        void remove_node(h32<node_graph_node> nodeHandle);

        void fetch_in_pins(h32<node_graph_node> nodeHandle, dynamic_array<h32<node_graph_in_pin>>& pins);
        void fetch_out_pins(h32<node_graph_node> nodeHandle, dynamic_array<h32<node_graph_out_pin>>& pins);

        bool can_connect(h32<node_graph_in_pin> in, h32<node_graph_out_pin> out) const;
        bool connect(h32<node_graph_in_pin> in, h32<node_graph_out_pin> out);

    private:
        friend class node_graph_context;

    private:
        const node_graph_registry* m_registry{};
        graph_type m_graph;
    };

    class node_graph_context
    {
    public:
        node_graph_context(node_graph& g, node_graph::graph_type::vertex_handle node);

        h32<node_graph_in_pin> add_in_pin(const pin_descriptor& desc) const;
        h32<node_graph_out_pin> add_out_pin(const pin_descriptor& desc) const;

    private:
        node_graph::graph_type* m_graph{};
        node_graph::graph_type::vertex_handle m_node{};
    };
}