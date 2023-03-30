#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/fixed_string.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/render_graph/render_graph.hpp>

#include <concepts>
#include <span>
#include <unordered_map>

namespace oblo
{
    template <typename T>
    concept is_compatible_render_graph_node =
        std::is_standard_layout_v<T> && std::is_trivially_destructible_v<T> && std::is_default_constructible_v<T>;

    template <typename T>
    concept is_compatible_render_graph_pin =
        std::is_trivially_destructible_v<T> && std::is_trivially_default_constructible_v<T>;

    template <typename T, fixed_string Name>
    struct render_node_in
    {
        static constexpr std::string_view name()
        {
            return std::string_view{Name.string};
        }

        const T* data;
    };

    template <typename T, fixed_string Name>
    struct render_node_out
    {
        static constexpr std::string_view name()
        {
            return std::string_view{Name.string};
        }

        T* data;
    };

    class render_graph_builder
    {
    public:
        template <typename T>
        render_graph_builder& add_node() requires is_compatible_render_graph_node<T>;

        template <typename T, typename NodeFrom, fixed_string NameFrom, typename NodeTo, fixed_string NameTo>
        render_graph_builder& add_edge(render_node_out<T, NameFrom>(NodeFrom::*from),
                                       render_node_in<T, NameTo>(NodeTo::*to));

        template <typename T>
        render_graph_builder& add_broadcast_input(std::string_view name);

        render_graph build() const;

    private:
        struct node_pin
        {
            u32 size;
            u32 alignment;
            u32 offset;
            u32 nextConnectedPin;
            u32 nodeIndex;
            type_id typeId;
            std::string_view name;
            void (*initialize)(void*);
        };

        struct node_type
        {
            u32 size;
            u32 alignment;
            u32 inputsBegin;
            u32 inputsEnd;
            u32 outputsBegin;
            u32 outputsEnd;
            void (*initialize)(void*);
            void (*execute)(void*);
        };

        struct edge_info
        {
            type_id fromNode;
            type_id toNode;
            u32 fromOffset;
            u32 toOffset;
        };

    private:
        static constexpr u32 Invalid{~u32{0}};

        template <typename Node, typename Member, fixed_string Name>
        void add_input_pin(const Node& baseNode, const render_node_in<Member, Name>& member, u32 nodeIndex)
        {
            const u32 offset =
                narrow_cast<u32>(reinterpret_cast<uintptr>(&member) - reinterpret_cast<uintptr>(&baseNode));

            m_pins.push_back({
                .size = sizeof(Member),
                .alignment = alignof(Member),
                .offset = offset,
                .nextConnectedPin = Invalid,
                .nodeIndex = nodeIndex,
                .typeId = get_type_id<Member>(),
                .name = member.name(),
                .initialize = [](void* ptr) { new (ptr) Member{}; },
            });
        }

        template <typename Node, typename Member, fixed_string Name>
        void add_output_pin(const Node& baseNode, const render_node_out<Member, Name>& member, u32 nodeIndex)
        {
            const u32 offset =
                narrow_cast<u32>(reinterpret_cast<uintptr>(&member) - reinterpret_cast<uintptr>(&baseNode));

            m_pins.push_back({
                .size = sizeof(Member),
                .alignment = alignof(Member),
                .offset = offset,
                .nextConnectedPin = Invalid,
                .nodeIndex = nodeIndex,
                .typeId = get_type_id<Member>(),
                .name = member.name(),
                .initialize = [](void* ptr) { new (ptr) Member{}; },
            });
        }

        // These are just fallbacks meant to ignore regular fields in nodes

        template <typename Node, typename T>
        constexpr void add_input_pin(const Node&, const T&, u32) const
        {
        }

        template <typename Node, typename T>
        constexpr void add_output_pin(const Node&, const T&, u32) const
        {
        }

        void add_edge_impl(
            node_type& nodeFrom, std::string_view pinFrom, node_type& nodeTo, std::string_view pinTo, type_id type);

    private:
        struct type_id_hash
        {
            constexpr auto operator()(const type_id& typeId) const
            {
                return std::hash<std::string_view>{}(typeId.name);
            }
        };

    private:
        std::unordered_map<type_id, node_type, type_id_hash> m_nodeTypes;
        std::vector<node_pin> m_pins;
        std::vector<node_pin> m_broadcastPins;
    };

    template <typename T>
    render_graph_builder& render_graph_builder::add_node() requires is_compatible_render_graph_node<T>
    {
        constexpr auto typeId = get_type_id<T>();
        const auto nodeIndex = narrow_cast<u32>(m_nodeTypes.size());

        const auto [it, ok] = m_nodeTypes.emplace(typeId,
                                                  node_type{
                                                      .size = sizeof(T),
                                                      .alignment = alignof(T),
                                                      .initialize = [](void* ptr) { new (ptr) T{}; },
                                                      .execute = [](void* ptr) { static_cast<T*>(ptr)->execute(); },
                                                  });

        OBLO_ASSERT(ok);

        const T baseNode{};

        node_type& node = it->second;

        node.inputsBegin = narrow_cast<u32>(m_pins.size());
        struct_apply([this, &baseNode, nodeIndex](auto&... pins)
                     { (this->add_input_pin(baseNode, pins, nodeIndex), ...); },
                     baseNode);
        node.inputsEnd = narrow_cast<u32>(m_pins.size());

        node.outputsBegin = narrow_cast<u32>(m_pins.size());
        struct_apply([this, &baseNode, nodeIndex](auto&... pins)
                     { (this->add_output_pin(baseNode, pins, nodeIndex), ...); },
                     baseNode);
        node.outputsEnd = narrow_cast<u32>(m_pins.size());

        return *this;
    }

    template <typename T, typename NodeFrom, fixed_string NameFrom, typename NodeTo, fixed_string NameTo>
    render_graph_builder& render_graph_builder::add_edge(render_node_out<T, NameFrom>(NodeFrom::*),
                                                         render_node_in<T, NameTo>(NodeTo::*))
    {
        constexpr auto t1 = get_type_id<NodeFrom>();
        constexpr auto t2 = get_type_id<NodeTo>();

        const auto it1 = m_nodeTypes.find(t1);
        const auto it2 = m_nodeTypes.find(t2);

        OBLO_ASSERT(m_nodeTypes.end() != it1 && m_nodeTypes.end() != it2);

        node_type& n1 = it1->second;
        node_type& n2 = it2->second;

        add_edge_impl(n1, NameFrom.string, n2, NameTo.string, get_type_id<T>());

        return *this;
    }

    template <typename T>
    render_graph_builder& render_graph_builder::add_broadcast_input(std::string_view name)
    {
        m_broadcastPins.push_back({
            .size = sizeof(T),
            .alignment = alignof(T),
            .offset = Invalid,
            .nextConnectedPin = Invalid,
            .nodeIndex = Invalid,
            .typeId = get_type_id<T>(),
            .name = name,
            .initialize = [](void* ptr) { new (ptr) T{}; },
        });

        return *this;
    }
}