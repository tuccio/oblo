#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/unique_ptr.hpp>

#include <iosfwd>
#include <utility>

namespace oblo
{
    template <typename>
    class deque;
}

namespace oblo::vk
{
    class frame_graph_template;

    class renderer;
    class vulkan_context;

    struct frame_graph_impl;
    struct frame_graph_subgraph;
    struct frame_graph_output_desc;

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
        void remove(h32<frame_graph_subgraph> graph);

        bool connect(h32<frame_graph_subgraph> srcGraph,
            string_view srcName,
            h32<frame_graph_subgraph> dstGraph,
            string_view dstName);

        template <typename T>
        expected<> set_input(h32<frame_graph_subgraph> graph, string_view name, T&& value);

        template <typename T>
        expected<T*> get_output(h32<frame_graph_subgraph> graph, string_view name);

        void disable_all_outputs(h32<frame_graph_subgraph> graph);
        void set_output_state(h32<frame_graph_subgraph> graph, string_view name, bool enable);

        bool init(vulkan_context& ctx);
        void shutdown(vulkan_context& ctx);

        void build(renderer& renderer);

        void execute(renderer& renderer);

        void write_dot(std::ostream& os) const;

        template <typename T>
            requires std::is_empty_v<T>
        void push_event(const T& e);

        void fetch_subgraphs(deque<h32<frame_graph_subgraph>>& outSubgraphs);
        void fetch_outputs(h32<frame_graph_subgraph> subgraph, deque<frame_graph_output_desc>& outSubgraphOutputs);

    private:
        void* try_get_input(h32<frame_graph_subgraph> graph, string_view name, const type_id& typeId);
        void* try_get_output(h32<frame_graph_subgraph> graph, string_view name, const type_id& typeId);

        void push_empty_event_impl(const type_id& type);
        bool has_event_impl(const type_id& type) const;

    private:
        unique_ptr<frame_graph_impl> m_impl;
    };

    template <typename T>
    expected<> frame_graph::set_input(h32<frame_graph_subgraph> graph, string_view name, T&& value)
    {
        using type = std::decay_t<T>;

        auto* const dst = try_get_input(graph, name, get_type_id<type>());

        if (type* const concrete = static_cast<type*>(dst))
        {
            *concrete = std::forward<T>(value);
            return no_error;
        }

        return unspecified_error;
    }

    template <typename T>
    expected<T*> frame_graph::get_output(h32<frame_graph_subgraph> graph, string_view name)
    {
        using type = std::decay_t<T>;

        auto* const dst = try_get_output(graph, name, get_type_id<type>());

        if (type* const concrete = static_cast<type*>(dst))
        {
            return concrete;
        }

        return unspecified_error;
    }

    template <typename T>
        requires std::is_empty_v<T>
    void frame_graph::push_event(const T&)
    {
        push_empty_event_impl(get_type_id<T>());
    }

    struct frame_graph_output_desc
    {
        string_view name;
        type_id type;
    };
}