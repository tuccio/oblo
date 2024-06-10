#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <iosfwd>
#include <memory>
#include <string_view>
#include <utility>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk
{
    class frame_graph_template;

    class renderer;
    class vulkan_context;

    struct frame_graph_impl;
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
        void remove(h32<frame_graph_subgraph> graph);

        bool connect(h32<frame_graph_subgraph> srcGraph,
            std::string_view srcName,
            h32<frame_graph_subgraph> dstGraph,
            std::string_view dstName);

        template <typename T>
        expected<> set_input(h32<frame_graph_subgraph> graph, std::string_view name, T&& value);

        bool init(vulkan_context& ctx);
        void shutdown(vulkan_context& ctx);

        void build(renderer& renderer);

        void execute(renderer& renderer);

        void write_dot(std::ostream& os) const;

    private:
        void* try_get_input(h32<frame_graph_subgraph> graph, std::string_view name, const type_id& typeId);

    private:
        std::unique_ptr<frame_graph_impl> m_impl;
    };

    template <typename T>
    expected<> frame_graph::set_input(h32<frame_graph_subgraph> graph, std::string_view name, T&& value)
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
}