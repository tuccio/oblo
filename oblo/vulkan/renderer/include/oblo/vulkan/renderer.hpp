#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/string_interner.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/draw/pass_manager.hpp>
#include <oblo/vulkan/draw/texture_registry.hpp>
#include <oblo/vulkan/graph/resource_pool.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/staging_buffer.hpp>

#include <memory>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::ecs
{
    class entity_registry;
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
        void update(frame_allocator& frameAllocator);

        vulkan_context& get_vulkan_context();
        single_queue_engine& get_engine();
        gpu_allocator& get_allocator();
        resource_manager& get_resource_manager();
        string_interner& get_string_interner();
        pass_manager& get_pass_manager();
        draw_registry& get_draw_registry();
        staging_buffer& get_staging_buffer();
        stateful_command_buffer& get_active_command_buffer();
        texture_registry& get_texture_registry();

        h32<render_graph> add(render_graph&& graph);
        void remove(h32<render_graph> graph);
        render_graph* find(h32<render_graph> graph);

    private:
        struct wrapped_render_graph;

    private:
        vulkan_context* m_vkContext{nullptr};

        staging_buffer m_stagingBuffer;
        draw_registry m_drawRegistry;
        texture_registry m_textureRegistry;

        string_interner m_stringInterner;
        pass_manager m_passManager;

        h32<buffer> m_dummy;

        resource_pool m_graphResourcePool;
        h32_flat_pool_dense_map<render_graph, wrapped_render_graph> m_renderGraphs;
    };

    struct renderer::initializer
    {
        vulkan_context& vkContext;
        frame_allocator& frameAllocator;
        ecs::entity_registry& entities;
    };

    inline vulkan_context& renderer::get_vulkan_context()
    {
        return *m_vkContext;
    }

    inline string_interner& renderer::get_string_interner()
    {
        return m_stringInterner;
    }

    inline pass_manager& renderer::get_pass_manager()
    {
        return m_passManager;
    }

    inline draw_registry& renderer::get_draw_registry()
    {
        return m_drawRegistry;
    }

    inline staging_buffer& renderer::get_staging_buffer()
    {
        return m_stagingBuffer;
    }

    inline texture_registry& renderer::get_texture_registry()
    {
        return m_textureRegistry;
    }
}