#pragma once

#include <oblo/core/graph/directed_graph.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/vulkan/graph/frame_graph_registry.hpp>
#include <oblo/vulkan/graph/frame_graph_vertex_kind.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <string>
#include <string_view>

namespace oblo::vk
{
    struct frame_graph_node_desc;

    struct frame_graph_template_vertex;

    using frame_graph_template_topology = directed_graph<frame_graph_template_vertex>;
    using frame_graph_template_vertex_handle = frame_graph_template_topology::vertex_handle;
    using frame_graph_template_edge_handle = frame_graph_template_topology::edge_handle;

    struct frame_graph_template_vertex
    {
        /// Determines whether the vertex is a node or a pin.
        frame_graph_vertex_kind kind;

        // TODO: Should maybe store the index in the pins array instead?
        /// Only valid for pins, it's the offset of the member in the node struct.
        u32 pinMemberOffset;

        /// For nodes, the id of the node. For pins, the id of the node it belongs to.
        /// Input and output nodes don't belong to any node, so they will have a nil uuid.
        uuid nodeId;

        /// For pins, the handle of the node;
        frame_graph_template_vertex_handle nodeHandle;

        /// Name of the node or pin, if any.
        std::string name;

        /// Node descriptor, only for node vertices.
        frame_graph_node_desc nodeDesc;

        /// Data desc, only for pins.
        frame_graph_data_desc pinDesc;
    };

    class frame_graph_template
    {
    public:
        using topology = frame_graph_template_topology;
        using vertex_handle = topology::vertex_handle;
        using edge_handle = topology::edge_handle;
        using edge_reference = topology::edge_reference;

    public:
        void init(const frame_graph_registry& registry);

        void make_input(vertex_handle pin, std::string_view name);

        template <typename R, typename Node>
        void make_input(vertex_handle node, R(Node::*pin), std::string_view name);

        void make_output(vertex_handle pin, std::string_view name);

        template <typename R, typename Node>
        void make_output(vertex_handle node, R(Node::*pin), std::string_view name);

        vertex_handle add_node(const uuid& id, const frame_graph_node_desc& desc);

        template <typename T>
        vertex_handle add_node();

        bool connect(vertex_handle srcPin, vertex_handle dstPin);

        template <typename T, typename NodeFrom, typename NodeTo>
        bool connect(vertex_handle src, resource<T>(NodeFrom::*from), vertex_handle dst, resource<T>(NodeTo::*to));

        template <typename T, typename NodeFrom, typename NodeTo>
        bool connect(vertex_handle src, data<T>(NodeFrom::*from), vertex_handle dst, data<T>(NodeTo::*to));

        const topology& get_graph() const;

        std::span<const edge_reference> get_inputs() const;
        std::span<const edge_reference> get_outputs() const;

        std::string_view get_name(edge_reference inputOrOutput) const;

    private:
        vertex_handle find_pin(vertex_handle node, u32 offset) const;

        template <typename R, typename Node>
        static u32 calculate_offset(R(Node::*m));

        static u32 calculate_offset(const u8* base, const u8* member);

    private:
        const frame_graph_registry* m_registry{};
        vertex_handle m_input{};
        vertex_handle m_output{};
        topology m_graph;
    };

    template <typename R, typename Node>
    void frame_graph_template::make_input(vertex_handle node, R(Node::*pin), std::string_view name)
    {
        // TODO: Could maybe somehow check that the template types match
        const auto pinVertex = find_pin(node, calculate_offset(pin));
        make_input(pinVertex, name);
    }

    template <typename R, typename Node>
    void frame_graph_template::make_output(vertex_handle node, R(Node::*pin), std::string_view name)
    {
        // TODO: Could maybe somehow check that the template types match
        const auto pinVertex = find_pin(node, calculate_offset(pin));
        make_output(pinVertex, name);
    }

    template <typename T>
    frame_graph_template::vertex_handle frame_graph_template::add_node()
    {
        const auto id = m_registry->get_uuid<T>();
        auto* const desc = m_registry->find_node(id);
        OBLO_ASSERT(desc);

        vertex_handle v{};

        if (desc)
        {
            v = add_node(id, *desc);
            m_graph[v].name = get_type_id<T>().name;
        }

        return v;
    }

    template <typename T, typename NodeFrom, typename NodeTo>
    bool frame_graph_template::connect(
        vertex_handle src, resource<T>(NodeFrom::*from), vertex_handle dst, resource<T>(NodeTo::*to))
    {
        const auto srcPin = find_pin(src, calculate_offset(from));
        const auto dstPin = find_pin(dst, calculate_offset(to));
        return connect(srcPin, dstPin);
    }

    template <typename T, typename NodeFrom, typename NodeTo>
    bool frame_graph_template::connect(
        vertex_handle src, data<T>(NodeFrom::*from), vertex_handle dst, data<T>(NodeTo::*to))
    {
        const auto srcPin = find_pin(src, calculate_offset(from));
        const auto dstPin = find_pin(dst, calculate_offset(to));
        return connect(srcPin, dstPin);
    }

    template <typename R, typename Node>
    u32 frame_graph_template::calculate_offset(R(Node::*m))
    {
        alignas(Node) u8 buffer[sizeof(Node)];
        R* const ptr = &(reinterpret_cast<Node*>(buffer)->*m);
        return calculate_offset(buffer, reinterpret_cast<const u8*>(ptr));
    }

    inline u32 frame_graph_template::calculate_offset(const u8* base, const u8* member)
    {
        return u32(member - base);
    }
}