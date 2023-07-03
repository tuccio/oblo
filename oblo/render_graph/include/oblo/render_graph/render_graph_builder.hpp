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
            u32 connectionOffset;
            type_id typeId;
            std::string_view name;
            void (*construct)(void*);
        };

        struct node_type
        {
            u32 size;
            u32 alignment;
            u32 inputsBegin;
            u32 inputsEnd;
            u32 outputsBegin;
            u32 outputsEnd;
            void (*construct)(void*);
            render_graph_initialize initialize;
            render_graph_execute execute;
            render_graph_shutdown shutdown;
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
                .construct = [](void* ptr) { new (ptr) Member{}; },
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
                .construct = [](void* ptr) { new (ptr) Member{}; },
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

        void add_edge_impl(std::span<node_pin> from,
                           std::span<node_pin> to,
                           std::string_view pinFrom,
                           std::string_view pinTo,
                           type_id typeFrom,
                           type_id typeTo,
                           u32 connectionOffset);

    protected:
        struct type_id_hash
        {
            auto operator()(const type_id& typeId) const noexcept
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
        render_graph_builder& add_node()
            requires is_compatible_render_graph_node<T, Context>;

        template <typename T, typename NodeFrom, fixed_string NameFrom, typename NodeTo, fixed_string NameTo>
        render_graph_builder& connect(render_node_out<T, NameFrom>(NodeFrom::*from),
                                      render_node_in<T, NameTo>(NodeTo::*to));

        template <typename T1,
                  typename T2,
                  typename NodeFrom,
                  fixed_string NameFrom,
                  typename NodeTo,
                  fixed_string NameTo,
                  typename Extract>
        render_graph_builder& connect(render_node_out<T1, NameFrom>(NodeFrom::*from),
                                      render_node_in<T2, NameTo>(NodeTo::*to),
                                      Extract&& extract);

        template <typename T>
        render_graph_builder& add_input(std::string_view name);

        template <typename TInput,
                  typename TPin,
                  typename NodeTo,
                  fixed_string NameTo,
                  typename Extract = std::nullptr_t>
        render_graph_builder& connect_input(std::string_view inputName,
                                            render_node_in<TPin, NameTo>(NodeTo::*to),
                                            Extract&& extract = nullptr);

        using render_graph_builder_impl::build;
    };

    inline std::error_code make_error_code(render_graph_builder_error e)
    {
        return std::error_code{static_cast<int>(e), render_graph_builder_impl::error_category()};
    }

    template <typename Context>
    template <typename T>
    render_graph_builder<Context>& render_graph_builder<Context>::add_node()
        requires is_compatible_render_graph_node<T, Context>
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
                                    .construct = [](void* ptr) { new (ptr) T{}; },
                                    .execute = [](void* ptr, void* context)
                                    { static_cast<T*>(ptr)->execute(static_cast<Context*>(context)); },
                                });

        if constexpr (requires(T t, Context* c) { bool{t.initialize(c)}; })
        {
            it->second.initialize = [](void* ptr, void* context) -> bool
            { return static_cast<T*>(ptr)->initialize(static_cast<Context*>(context)); };
        }

        if constexpr (requires(T t, Context* c) { t.shutdown(c); })
        {
            it->second.shutdown = [](void* ptr, void* context)
            { static_cast<T*>(ptr)->shutdown(static_cast<Context*>(context)); };
        }

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
    render_graph_builder<Context>& render_graph_builder<Context>::connect(render_node_out<T, NameFrom>(NodeFrom::*),
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
            node_type& nodeFrom = it1->second;
            node_type& nodeTo = it2->second;

            const auto from =
                std::span{m_pins}.subspan(nodeFrom.outputsBegin, nodeFrom.outputsEnd - nodeFrom.outputsBegin);
            const auto to = std::span{m_pins}.subspan(nodeTo.inputsBegin, nodeTo.inputsEnd - nodeTo.inputsBegin);

            add_edge_impl(from, to, NameFrom.string, NameTo.string, get_type_id<T>(), get_type_id<T>(), 0u);
        }
        else
        {
            m_lastError = render_graph_builder_error::node_not_found;
        }

        return *this;
    }

    template <typename Context>
    template <typename T1,
              typename T2,
              typename NodeFrom,
              fixed_string NameFrom,
              typename NodeTo,
              fixed_string NameTo,
              typename Extract>
    render_graph_builder<Context>& render_graph_builder<Context>::connect(render_node_out<T1, NameFrom>(NodeFrom::*),
                                                                          render_node_in<T2, NameTo>(NodeTo::*),
                                                                          Extract&& extract)
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
            node_type& nodeFrom = it1->second;
            node_type& nodeTo = it2->second;

            const T1 outputData{};
            const T2* extractedPtr = extract(&outputData);
            const auto offset = narrow_cast<u32>(reinterpret_cast<const std::byte*>(extractedPtr) -
                                                 reinterpret_cast<const std::byte*>(&outputData));

            const auto from =
                std::span{m_pins}.subspan(nodeFrom.outputsBegin, nodeFrom.outputsEnd - nodeFrom.outputsBegin);
            const auto to = std::span{m_pins}.subspan(nodeTo.inputsBegin, nodeTo.inputsEnd - nodeTo.inputsBegin);

            add_edge_impl(from, to, NameFrom.string, NameTo.string, get_type_id<T1>(), get_type_id<T2>(), offset);
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
            .construct = [](void* ptr) { new (ptr) T{}; },
        });

        return *this;
    }

    template <typename Context>
    template <typename TInput, typename TPin, typename NodeTo, fixed_string NameTo, typename Extract>
    render_graph_builder<Context>& render_graph_builder<Context>::connect_input(std::string_view inputName,
                                                                                render_node_in<TPin, NameTo>(NodeTo::*),
                                                                                Extract&& extract)
    {
        if (m_lastError)
        {
            return *this;
        }

        constexpr auto type = get_type_id<NodeTo>();

        const auto it = m_nodeTypes.find(type);

        if (m_nodeTypes.end() != it)
        {
            node_type& nodeTo = it->second;

            u32 offset{0};

            if constexpr (requires(Extract& e, const TInput& in, const TPin* r) { r = e(in); })
            {
                const TInput inputData{};
                const TPin* extractedPtr{extract(&inputData)};
                offset = narrow_cast<u32>(reinterpret_cast<const std::byte*>(extractedPtr) -
                                          reinterpret_cast<const std::byte*>(&inputData));
            }

            const auto from = std::span{m_broadcastPins};
            const auto to = std::span{m_pins}.subspan(nodeTo.inputsBegin, nodeTo.inputsEnd - nodeTo.inputsBegin);

            add_edge_impl(from, to, inputName, NameTo.string, get_type_id<TInput>(), get_type_id<TPin>(), offset);
        }
        else
        {
            m_lastError = render_graph_builder_error::node_not_found;
        }

        return *this;
    }

}

template <>
struct std::is_error_code_enum<oblo::render_graph_builder_error> : true_type
{
};