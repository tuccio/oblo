#pragma once

#include <oblo/core/graph/directed_graph.hpp>

#include <span>

namespace oblo
{
    class cstring_view;
    class data_document;
    class node_graph_context;
    class node_graph_registry;

    struct node_graph_node;
    struct node_graph_in_pin;
    struct node_graph_out_pin;
    struct pin_descriptor;
    struct uuid;
    struct vec2;

    class node_graph
    {
    public:
        struct edge_type;
        struct vertex_type;

        using graph_type = directed_graph<vertex_type, edge_type>;

        class nodes_iterator;

    public:
        node_graph();
        node_graph(const node_graph&) = delete;
        node_graph(node_graph&&) noexcept;
        ~node_graph();

        node_graph& operator=(const node_graph&) = delete;
        node_graph& operator=(node_graph&&) noexcept;

        void init(const node_graph_registry& registry);

        const node_graph_registry& get_registry() const;

        h32<node_graph_node> add_node(const uuid& id);
        void remove_node(h32<node_graph_node> nodeHandle);

        void fetch_nodes(dynamic_array<h32<node_graph_node>>& nodes) const;

        void fetch_in_pins(h32<node_graph_node> nodeHandle, dynamic_array<h32<node_graph_in_pin>>& pins) const;
        void fetch_out_pins(h32<node_graph_node> nodeHandle, dynamic_array<h32<node_graph_out_pin>>& pins) const;

        void store(h32<node_graph_node> nodeHandle, data_document& doc, u32 docNodeIndex) const;
        void load(h32<node_graph_node> nodeHandle, const data_document& doc, u32 docNodeIndex);

        bool can_connect(h32<node_graph_out_pin> src, h32<node_graph_in_pin> dst) const;
        bool connect(h32<node_graph_out_pin> src, h32<node_graph_in_pin> dst);

        uuid get_type(h32<node_graph_node> nodeHandle) const;

        const vec2& get_ui_position(h32<node_graph_node> nodeHandle) const;
        void set_ui_position(h32<node_graph_node> nodeHandle, const vec2& position);

        cstring_view get_name(h32<node_graph_in_pin> pin) const;
        cstring_view get_name(h32<node_graph_out_pin> pin) const;

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

        void fetch_in_pins(dynamic_array<h32<node_graph_in_pin>>& pins) const;
        void fetch_out_pins(dynamic_array<h32<node_graph_out_pin>>& pins) const;

        void mark_modified(h32<node_graph_in_pin> h) const;
        void mark_modified(h32<node_graph_out_pin> h) const;

    private:
        node_graph::graph_type* m_graph{};
        node_graph::graph_type::vertex_handle m_node{};
    };
}