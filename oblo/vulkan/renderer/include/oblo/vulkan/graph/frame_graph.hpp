#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <vulkan/vulkan_core.h>

#include <memory>
#include <string_view>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk
{
    class frame_graph_build_context;
    class frame_graph_execute_context;
    class frame_graph_init_context;
    class frame_graph_template;
    class renderer;
    class resource_pool;

    struct frame_graph_pin;
    struct frame_graph_pin_storage;
    struct frame_graph_subgraph;

    struct buffer;
    struct texture;
    struct staging_buffer_span;

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

        void add_transient_resource(resource<texture> handle, u32 poolIndex);
        void add_resource_transition(resource<texture> handle, VkImageLayout target);

        u32 find_pool_index(resource<texture> handle) const;
        u32 find_pool_index(resource<buffer> handle) const;

        void add_transient_buffer(resource<buffer> handle, u32 poolIndex, const staging_buffer_span* upload);
        void add_buffer_access(resource<buffer> handle, VkPipelineStageFlags2 pipelineStage, VkAccessFlags2 access);

        h32<frame_graph_pin_storage> allocate_dynamic_resource_pin();

        frame_allocator& get_frame_allocator() const;

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