#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/string_interner.hpp>
#include <oblo/vulkan/graph/resource_pool.hpp>
#include <oblo/vulkan/mesh_table.hpp>
#include <oblo/vulkan/render_pass_manager.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/staging_buffer.hpp>

#include <memory>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk
{
    class render_graph;
    class resource_pool;

    class vulkan_context;
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
        void shutdown();
        void update();

        vulkan_context& get_vulkan_context();
        single_queue_engine& get_engine();
        allocator& get_allocator();
        resource_manager& get_resource_manager();
        string_interner& get_string_interner();
        render_pass_manager& get_render_pass_manager();
        mesh_table& get_mesh_table();
        staging_buffer& get_staging_buffer();
        stateful_command_buffer& get_active_command_buffer();

        h32<render_graph> add(render_graph&& graph);
        void remove(h32<render_graph> graph);
        render_graph* find(h32<render_graph> graph);

    private:
        struct wrapped_render_graph;

    private:
        vulkan_context* m_vkContext{nullptr};

        mesh_table m_meshes;
        staging_buffer m_stagingBuffer;

        string_interner m_stringInterner;
        render_pass_manager m_renderPassManager;

        h32<buffer> m_dummy;
        u32 m_lastRenderGraphId{};

        resource_pool m_graphResourcePool;
        flat_dense_map<h32<render_graph>, wrapped_render_graph> m_renderGraphs;
    };

    struct renderer::initializer
    {
        vulkan_context& vkContext;
        frame_allocator& frameAllocator;
    };

    inline vulkan_context& renderer::get_vulkan_context()
    {
        return *m_vkContext;
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