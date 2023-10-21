#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/lifetime.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/vulkan/graph/render_graph_node.hpp>

#include <concepts>
#include <span>
#include <system_error>
#include <unordered_map>

namespace oblo::vk
{
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
        struct node_type
        {
            u32 size;
            u32 alignment;
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

        struct connected_input
        {
            type_id type;
            u32 offset;
        };

        struct graph_input
        {
            std::string_view name;
            type_id type;
            std::vector<connected_input> connections;
        };

    protected:
        static constexpr u32 Invalid{~u32{0}};

        std::error_code build_graph(render_graph& graph) const;
        std::error_code build_executor(const render_graph& graph, render_graph_seq_executor& executor) const;

        template <typename T, typename U>
        static u32 get_member_offset(U(T::*m))
        {
            alignas(T) std::byte buf[sizeof(T)];
            auto& t = *start_lifetime_as<T>(buf);

            u8* const bStructPtr = reinterpret_cast<u8*>(&t);
            u8* const bMemberPtr = reinterpret_cast<u8*>(&(t.*m));
            return u32(bMemberPtr - bStructPtr);
        }

    protected:
        std::error_code m_lastError{};
        std::unordered_map<type_id, node_type> m_nodeTypes;
        std::vector<graph_input> m_graphInputs;
        std::vector<edge_info> m_edges;
    };

    template <typename Context>
    class render_graph_builder : render_graph_builder_impl
    {
    public:
        template <typename T>
        render_graph_builder& add_node()
            requires is_compatible_render_graph_node<T, Context>;

        template <typename T, typename NodeFrom, typename NodeTo>
        render_graph_builder& connect(T(NodeFrom::*from), T(NodeTo::*to));

        template <typename T>
        render_graph_builder& add_input(std::string_view name);

        template <typename T, typename NodeTo>
        render_graph_builder& connect_input(std::string_view inputName, T(NodeTo::*to));

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

        const auto [it, ok] = m_nodeTypes.emplace(typeId,
            node_type{
                .size = sizeof(T),
                .alignment = alignof(T),
                .construct = [](void* ptr) { new (ptr) T{}; },
                .execute = [](void* ptr, void* context)
                { static_cast<T*>(ptr)->execute(static_cast<Context*>(context)); },
            });

        OBLO_ASSERT(ok);

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

        return *this;
    }

    template <typename Context>
    template <typename T, typename NodeFrom, typename NodeTo>
    render_graph_builder<Context>& render_graph_builder<Context>::connect(T(NodeFrom::*from), T(NodeTo::*to))
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
            const u32 fromOffset = get_member_offset(from);
            const u32 toOffset = get_member_offset(to);

            m_edges.push_back({
                .fromNode = t1,
                .toNode = t2,
                .fromOffset = fromOffset,
                .toOffset = toOffset,
            });
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
        // TODO
        (void) name;

        return *this;
    }

    template <typename Context>
    template <typename T, typename NodeTo>
    render_graph_builder<Context>& render_graph_builder<Context>::connect_input(std::string_view, T(NodeTo::*))
    {
        if (m_lastError)
        {
            return *this;
        }

        // TODO

        return *this;
    }
}

template <>
struct std::is_error_code_enum<oblo::vk::render_graph_builder_error> : true_type
{
};