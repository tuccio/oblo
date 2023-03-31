#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/fixed_string.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/render_graph/render_graph_node.hpp>

#include <concepts>
#include <span>
#include <system_error>
#include <unordered_map>

namespace oblo
{
    class render_graph;
    class render_graph_seq_executor;

    enum class render_graph_builder_error : u8
    {
        success,
        node_not_found,
        pin_not_found,
        missing_input,
        input_already_connected,
        not_a_dag,
    };

    class render_graph_builder_impl
    {
    public:
        std::error_code build(render_graph& graph, render_graph_seq_executor& executor) const;

        static const std::error_category& error_category();

    protected:
        struct node_pin
        {
            u32 size;
            u32 alignment;
            u32 offset;
            u32 nextConnectedInput;
            u32 connectedOutput;
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
            void (*execute)(void*, void*);
        };

        struct edge_info
        {
            type_id fromNode;
            type_id toNode;
            u32 fromOffset;
            u32 toOffset;
        };

    protected:
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
                .nextConnectedInput = Invalid,
                .connectedOutput = Invalid,
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
                .nextConnectedInput = Invalid,
                .connectedOutput = Invalid,
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

        std::error_code build_graph(render_graph& graph) const;
        std::error_code build_executor(const render_graph& graph, render_graph_seq_executor& executor) const;

        void add_edge_impl(
            node_type& nodeFrom, std::string_view pinFrom, node_type& nodeTo, std::string_view pinTo, type_id type);

    protected:
        struct type_id_hash
        {
            constexpr auto operator()(const type_id& typeId) const
            {
                return std::hash<std::string_view>{}(typeId.name);
            }
        };

    protected:
        std::error_code m_lastError{};
        std::unordered_map<type_id, node_type, type_id_hash> m_nodeTypes;
        std::vector<node_pin> m_pins;
        std::vector<node_pin> m_broadcastPins;
    };

    template <typename Context>
    class render_graph_builder : render_graph_builder_impl
    {
    public:
        template <typename T>
        render_graph_builder& add_node() requires is_compatible_render_graph_node<T, Context>;

        template <typename T, typename NodeFrom, fixed_string NameFrom, typename NodeTo, fixed_string NameTo>
        render_graph_builder& add_edge(render_node_out<T, NameFrom>(NodeFrom::*from),
                                       render_node_in<T, NameTo>(NodeTo::*to));

        template <typename T>
        render_graph_builder& add_input(std::string_view name);

        using render_graph_builder_impl::build;
    };

    inline std::error_code make_error_code(render_graph_builder_error e)
    {
        return std::error_code{static_cast<int>(e), render_graph_builder_impl::error_category()};
    }

    template <typename Context>
    template <typename T>
    render_graph_builder<Context>& render_graph_builder<Context>::add_node() requires
        is_compatible_render_graph_node<T, Context>
    {
        if (m_lastError)
        {
            return *this;
        }

        constexpr auto typeId = get_type_id<T>();
        const auto nodeIndex = narrow_cast<u32>(m_nodeTypes.size());

        const auto [it, ok] =
            m_nodeTypes.emplace(typeId,
                                node_type{
                                    .size = sizeof(T),
                                    .alignment = alignof(T),
                                    .initialize = [](void* ptr) { new (ptr) T{}; },
                                    .execute = [](void* ptr, void* context)
                                    { static_cast<T*>(ptr)->execute(static_cast<Context*>(context)); },
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

    template <typename Context>
    template <typename T, typename NodeFrom, fixed_string NameFrom, typename NodeTo, fixed_string NameTo>
    render_graph_builder<Context>& render_graph_builder<Context>::add_edge(render_node_out<T, NameFrom>(NodeFrom::*),
                                                                           render_node_in<T, NameTo>(NodeTo::*))
    {
        if (m_lastError)
        {
            return *this;
        }

        constexpr auto t1 = get_type_id<NodeFrom>();
        constexpr auto t2 = get_type_id<NodeTo>();

        const auto it1 = m_nodeTypes.find(t1);
        const auto it2 = m_nodeTypes.find(t2);

        if (m_nodeTypes.end() != it1 && m_nodeTypes.end() != it2)
        {
            node_type& n1 = it1->second;
            node_type& n2 = it2->second;

            add_edge_impl(n1, NameFrom.string, n2, NameTo.string, get_type_id<T>());
        }
        else
        {
            m_lastError = render_graph_builder_error::node_not_found;
        }

        return *this;
    }

    template <typename Context>
    template <typename T>
    render_graph_builder<Context>& render_graph_builder<Context>::add_input(std::string_view name)
    {
        if (m_lastError)
        {
            return *this;
        }

        m_broadcastPins.push_back({
            .size = sizeof(T),
            .alignment = alignof(T),
            .offset = Invalid,
            .nextConnectedInput = Invalid,
            .connectedOutput = Invalid,
            .nodeIndex = Invalid,
            .typeId = get_type_id<T>(),
            .name = name,
            .initialize = [](void* ptr) { new (ptr) T{}; },
        });

        return *this;
    }
}

template <>
struct std::is_error_code_enum<oblo::render_graph_builder_error> : true_type
{
};