#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/graph/directed_graph.hpp>

#include <span>

namespace oblo
{
    class abstract_syntax_tree;
    class cstring_view;
    class data_document;
    class node_graph_context;
    class node_graph_registry;

    struct node_graph_node;
    struct node_graph_in_pin;
    struct node_graph_out_pin;
    struct node_property_descriptor;
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

        void fetch_properties_descriptors(h32<node_graph_node> nodeHandle,
            dynamic_array<node_property_descriptor>& outPropertyDescriptors) const;

        void store(h32<node_graph_node> nodeHandle, data_document& doc, u32 docNodeIndex) const;
        void load(h32<node_graph_node> nodeHandle, const data_document& doc, u32 docNodeIndex);

        bool can_connect(h32<node_graph_out_pin> src, h32<node_graph_in_pin> dst) const;
        bool connect(h32<node_graph_out_pin> src, h32<node_graph_in_pin> dst);

        void clear_connected_output(h32<node_graph_in_pin> inPin);
        h32<node_graph_out_pin> get_connected_output(h32<node_graph_in_pin> inPin) const;

        uuid get_type(h32<node_graph_node> nodeHandle) const;

        const vec2& get_ui_position(h32<node_graph_node> nodeHandle) const;
        void set_ui_position(h32<node_graph_node> nodeHandle, const vec2& position);

        cstring_view get_name(h32<node_graph_in_pin> pin) const;
        cstring_view get_name(h32<node_graph_out_pin> pin) const;

        h32<node_graph_node> get_owner_node(h32<node_graph_in_pin> pin) const;
        h32<node_graph_node> get_owner_node(h32<node_graph_out_pin> pin) const;

        uuid get_deduced_type(h32<node_graph_in_pin> h) const;
        uuid get_deduced_type(h32<node_graph_out_pin> h) const;

        expected<> serialize(data_document& doc, u32 nodeIndex) const;
        expected<> deserialize(const data_document& doc, u32 nodeIndex);

        expected<> generate_ast(abstract_syntax_tree& ast) const;

    private:
        friend class node_graph_context;

    private:
        void call_on_input_change(dynamic_array<graph_type::vertex_handle>& root);

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

        uuid get_incoming_type(h32<node_graph_in_pin> h) const;

        uuid get_deduced_type(h32<node_graph_in_pin> h) const;
        void set_deduced_type(h32<node_graph_in_pin> h, const uuid& type) const;

        uuid get_deduced_type(h32<node_graph_out_pin> h) const;
        void set_deduced_type(h32<node_graph_out_pin> h, const uuid& type) const;

        uuid find_promotion_rule(const uuid& lhs, const uuid& rhs) const;

    private:
        const node_graph_registry* m_registry{};
        node_graph::graph_type* m_graph{};
        node_graph::graph_type::vertex_handle m_node{};
    };
}