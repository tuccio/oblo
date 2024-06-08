#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/type_id.hpp>

#include <memory>
#include <string_view>

namespace oblo::vk
{
    class frame_graph_build_context;
    class frame_graph_execute_context;
    class frame_graph_init_context;
    class frame_graph_template;
    class renderer;
    class resource_pool;
    struct frame_graph_pin_storage;
    struct frame_graph_subgraph;

    class frame_graph
    {
    public:
        frame_graph();
        frame_graph(const frame_graph&) = delete;
        frame_graph(frame_graph&&) noexcept;
        frame_graph& operator=(const frame_graph&) = delete;
        frame_graph& operator=(frame_graph&&) noexcept;
        ~frame_graph();

        h32<frame_graph_subgraph> instantiate(const frame_graph_template& graphTemplate);

        bool connect(h32<frame_graph_subgraph> srcGraph,
            std::string_view srcName,
            h32<frame_graph_subgraph> dstGraph,
            std::string_view dstName);

        template <typename T>
        bool set_input(h32<frame_graph_subgraph> graph, std::string_view name, const T& value);

        void init();

        void build(renderer& renderer, resource_pool& resourcePool);

        void execute(renderer& renderer, resource_pool& resourcePool);

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

    private:
        struct impl;

        friend class frame_graph_build_context;
        friend class frame_graph_execute_context;
        friend class frame_graph_init_context;

    private:
        void* try_get_input(h32<frame_graph_subgraph> graph, std::string_view name, const type_id& typeId);

    private:
        std::unique_ptr<impl> m_impl;
    };

    template <typename T>
    bool frame_graph::set_input(h32<frame_graph_subgraph> graph, std::string_view name, const T& value)
    {
        auto* const dst = try_get_input(graph, name, get_type_id<T>());

        if (dst)
        {
            *static_cast<T*>(dst) = value;
            return true;
        }

        return false;
    }
}