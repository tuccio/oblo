#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_interner.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/staging_buffer.hpp>
#include <oblo/vulkan/draw/pass_manager.hpp>
#include <oblo/vulkan/draw/resource_cache.hpp>
#include <oblo/vulkan/draw/texture_registry.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>

namespace oblo
{
    class instance_data_type_registry;
    struct renderer_platform;

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

        gpu::gpu_instance& get_gpu_instance();
        string_interner& get_string_interner();

        const instance_data_type_registry& get_instance_data_type_registry() const;

        gpu::staging_buffer& get_staging_buffer();

        frame_graph& get_frame_graph();

        bool is_ray_tracing_enabled() const;

        /// @brief Returns a handle to private platform-specific data.
        renderer_platform* get_renderer_platform();

    private:
        gpu::gpu_instance* m_gpu{};
        gpu::gpu_queue_context* m_gpuQueueCtx{};

        unique_ptr<instance_data_type_registry> m_instanceDataTypeRegistry;

        gpu::staging_buffer m_stagingBuffer;

        unique_ptr<renderer_platform> m_platform;

        string_interner m_stringInterner;

        frame_graph m_frameGraph;

        bool m_firstUpdate{};
        bool m_isRayTracingEnabled{};
    };

    struct renderer::initializer
    {
        gpu::gpu_queue_context& queueContext;
        bool isRayTracingEnabled;
    };

    inline gpu::gpu_instance& renderer::get_gpu_instance()
    {
        return *m_gpu;
    }

    inline string_interner& renderer::get_string_interner()
    {
        return m_stringInterner;
    }

    inline gpu::staging_buffer& renderer::get_staging_buffer()
    {
        return m_stagingBuffer;
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