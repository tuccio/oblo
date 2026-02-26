#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_interner.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/staging_buffer.hpp>
#include <oblo/renderer/graph/frame_graph.hpp>

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

        hptr<gpu::command_buffer> get_active_command_buffer();

        hptr<gpu::command_buffer> finalize_command_buffer_for_submission();

        // An opaque handle for frame graph arguments, it's only really here for unit testing purposing
        renderer_platform& get_renderer_platform();

    private:
        struct used_command_buffer_pool;

    private:
        gpu::gpu_instance* m_gpu{};

        unique_ptr<instance_data_type_registry> m_instanceDataTypeRegistry;

        gpu::staging_buffer m_stagingBuffer;

        unique_ptr<renderer_platform> m_platform;
        deque<used_command_buffer_pool> m_usedPools;

        string_interner m_stringInterner;

        frame_graph m_frameGraph;

        hptr<gpu::command_buffer> m_currentCmdBuffer{};
        h32<gpu::command_buffer_pool> m_currentCmdBufferPool{};

        bool m_firstUpdate{};
        bool m_isRayTracingEnabled{};
    };

    struct renderer::initializer
    {
        gpu::gpu_instance& gpu;
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

    inline frame_graph& renderer::get_frame_graph()
    {
        return m_frameGraph;
    }

    inline bool renderer::is_ray_tracing_enabled() const
    {
        return m_isRayTracingEnabled;
    }
}