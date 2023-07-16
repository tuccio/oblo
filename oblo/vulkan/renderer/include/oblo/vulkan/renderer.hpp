#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/string_interner.hpp>
#include <oblo/vulkan/mesh_table.hpp>
#include <oblo/vulkan/render_pass_manager.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/staging_buffer.hpp>

namespace oblo
{
    class frame_allocator;
    class render_graph;

    template <typename T>
    class render_graph_builder;
}

namespace oblo::vk
{
    struct buffer;

    class renderer
    {
    public:
        struct initializer;
        struct update_context;

    public:
        renderer();
        renderer(const renderer&) = delete;
        renderer(renderer&&) = delete;
        renderer& operator=(const renderer&) = delete;
        renderer& operator=(renderer&&) = delete;
        ~renderer();

        bool init(const initializer& initializer);
        void shutdown(frame_allocator& frameAllocator);
        void update(const update_context& context);

        single_queue_engine& get_engine();
        allocator& get_allocator();
        resource_manager& get_resource_manager();
        string_interner& get_string_interner();
        render_pass_manager& get_render_pass_manager();
        mesh_table& get_mesh_table();
        staging_buffer& get_staging_buffer();

        h32<render_graph> create_graph(const render_graph_builder<renderer_context>& builder,
                                       frame_allocator& frameAllocator);
        void destroy_graph(h32<render_graph> handle, frame_allocator& frameAllocator);
        render_graph* find_graph(h32<render_graph> handle);

    private:
        struct render_graph_data;

    private:
        single_queue_engine* m_engine{nullptr};
        allocator* m_allocator{nullptr};
        resource_manager* m_resourceManager{nullptr};

        mesh_table m_meshes;
        staging_buffer m_stagingBuffer;

        string_interner m_stringInterner;
        render_pass_manager m_renderPassManager;

        h32<buffer> m_dummy;
        u32 m_lastRenderGraphId{};

        flat_dense_map<h32<render_graph>, render_graph_data> m_renderGraphs;
    };

    struct renderer::initializer
    {
        single_queue_engine& engine;
        allocator& allocator;
        frame_allocator& frameAllocator;
        resource_manager& resourceManager;
    };

    struct renderer::update_context
    {
        stateful_command_buffer& commandBuffer;
        frame_allocator& frameAllocator;
    };

    inline single_queue_engine& renderer::get_engine()
    {
        return *m_engine;
    }

    inline allocator& renderer::get_allocator()
    {
        return *m_allocator;
    }

    inline resource_manager& renderer::get_resource_manager()
    {
        return *m_resourceManager;
    }

    inline string_interner& renderer::get_string_interner()
    {
        return m_stringInterner;
    }

    inline render_pass_manager& renderer::get_render_pass_manager()
    {
        return m_renderPassManager;
    }

    inline mesh_table& renderer::get_mesh_table()
    {
        return m_meshes;
    }

    inline staging_buffer& renderer::get_staging_buffer()
    {
        return m_stagingBuffer;
    }
}