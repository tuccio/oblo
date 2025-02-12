#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_interner.hpp>
#include <oblo/vulkan/draw/pass_manager.hpp>
#include <oblo/vulkan/draw/resource_cache.hpp>
#include <oblo/vulkan/draw/texture_registry.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/staging_buffer.hpp>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk
{
    class instance_data_type_registry;
    class resource_manager;
    class single_queue_engine;

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

        void begin_frame();
        void end_frame();

        vulkan_context& get_vulkan_context();
        single_queue_engine& get_engine();
        gpu_allocator& get_allocator();
        resource_manager& get_resource_manager();
        string_interner& get_string_interner();
        pass_manager& get_pass_manager();
        staging_buffer& get_staging_buffer();
        stateful_command_buffer& get_active_command_buffer();
        texture_registry& get_texture_registry();
        resource_cache& get_resource_cache();
        const instance_data_type_registry& get_instance_data_type_registry() const;

        frame_graph& get_frame_graph();

        bool is_ray_tracing_enabled() const;

    private:
        vulkan_context* m_vkContext{nullptr};

        unique_ptr<instance_data_type_registry> m_instanceDataTypeRegistry;

        staging_buffer m_stagingBuffer;
        texture_registry m_textureRegistry;
        resource_cache m_resourceCache;

        string_interner m_stringInterner;
        pass_manager m_passManager;

        h32<buffer> m_dummy;

        frame_graph m_frameGraph;

        bool m_firstUpdate{};
        bool m_isRayTracingEnabled{};
    };

    struct renderer::initializer
    {
        vulkan_context& vkContext;
        bool isRayTracingEnabled;
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

    inline staging_buffer& renderer::get_staging_buffer()
    {
        return m_stagingBuffer;
    }

    inline texture_registry& renderer::get_texture_registry()
    {
        return m_textureRegistry;
    }

    inline resource_cache& renderer::get_resource_cache()
    {
        return m_resourceCache;
    }

    inline frame_graph& vk::renderer::get_frame_graph()
    {
        return m_frameGraph;
    }

    inline bool vk::renderer::is_ray_tracing_enabled() const
    {
        return m_isRayTracingEnabled;
    }
}