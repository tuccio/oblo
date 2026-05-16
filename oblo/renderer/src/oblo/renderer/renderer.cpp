#include <oblo/renderer/renderer.hpp>

#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/renderer/draw/instance_data_type_registry.hpp>
#include <oblo/renderer/platform/renderer_platform.hpp>

namespace oblo
{
    constexpr u32 staging_buffer_size{1u << 30};
    constexpr u32 command_buffers_per_pool{8};

    renderer::renderer() = default;
    renderer::~renderer() = default;

    bool renderer::init(const renderer::initializer& initializer)
    {
        m_gpu = &initializer.gpu;

        m_isRayTracingEnabled = initializer.isRayTracingEnabled;

        m_platform = allocate_unique<renderer_platform>();

        if (!m_stagingBuffer.init(*m_gpu, staging_buffer_size))
        {
            return false;
        }

        if (!m_platform->textureRegistry.init(*m_gpu, m_stagingBuffer))
        {
            return false;
        }

        if (!m_frameGraph.init(*m_gpu))
        {
            return false;
        }

        m_stringInterner.init(256);

        m_instanceDataTypeRegistry = allocate_unique<instance_data_type_registry>();
        m_instanceDataTypeRegistry->register_from_module();

        m_platform->passManager.init(*m_gpu,
            m_stringInterner,
            m_platform->textureRegistry,
            *m_instanceDataTypeRegistry);

        m_platform->passManager.set_raytracing_enabled(m_isRayTracingEnabled);

        const string_view includePaths[] = {"./vulkan/shaders/", "./imgui/shaders"};
        m_platform->passManager.set_system_include_paths(includePaths);

        m_platform->resourceCache.init(m_platform->textureRegistry);

        m_commandBufferPools.init(*m_gpu, command_buffers_per_pool);

        m_firstUpdate = true;

        return true;
    }

    void renderer::shutdown()
    {
        if (m_gpu)
        {
            m_frameGraph.shutdown(*m_gpu);
            m_commandBufferPools.shutdown();
        }

        if (m_platform)
        {
            m_platform->passManager.shutdown();
            m_platform->textureRegistry.shutdown();
        }

        m_stagingBuffer.shutdown();
    }

    void renderer::begin_frame()
    {
        m_platform->resourceCache.update();

        m_stagingBuffer.notify_finished_frames(m_gpu->get_last_finished_submit());

        m_stagingBuffer.begin_submit();

        if (m_firstUpdate)
        {
            m_platform->textureRegistry.on_first_frame();

            m_firstUpdate = false;
        }
    }

    hptr<gpu::command_buffer> renderer::execute()
    {
        const hptr<gpu::command_buffer> commandBuffer = get_active_command_buffer();
        OBLO_ASSERT(commandBuffer);

        if (!commandBuffer)
        {
            // If we didn't get a command buffer it may be an unrecoverable error, but we try to keep going
            return {};
        }

        m_platform->textureRegistry.flush_uploads(commandBuffer);

        m_platform->passManager.begin_frame(commandBuffer);

        const frame_graph_build_args buildArgs{
            .rendererPlatform = *m_platform,
            .gpu = *m_gpu,
            .stagingBuffer = m_stagingBuffer,
        };

        m_frameGraph.build(buildArgs);

        // Frame graph building might update the texture descriptors, so we update them after that
        m_platform->passManager.update_global_descriptor_sets();

        const frame_graph_execute_args executeArgs{
            .rendererPlatform = *m_platform,
            .gpu = *m_gpu,
            .commandBuffer = commandBuffer,
            .stagingBuffer = m_stagingBuffer,
        };

        m_frameGraph.execute(executeArgs);

        m_platform->passManager.end_frame();

        if (commandBuffer)
        {
            OBLO_ASSERT(commandBuffer == m_currentCmdBuffer);
            m_gpu->end_command_buffer(commandBuffer).assert_value();
            m_currentCmdBuffer = {};
        }

        return commandBuffer;
    }

    void renderer::end_frame(u64 submitIndex)
    {
        m_stagingBuffer.end_submit(submitIndex);
        m_frameGraph.frame_submitted(*m_gpu, submitIndex);
        m_commandBufferPools.frame_submitted(submitIndex);
    }

    const instance_data_type_registry& renderer::get_instance_data_type_registry() const
    {
        return *m_instanceDataTypeRegistry;
    }

    void renderer::set_profiling_enabled(bool enable)
    {
        m_platform->passManager.set_profiling_enabled(enable);
    }

    bool renderer::is_profiling_enabled() const
    {
        return m_platform->passManager.is_profiling_enabled();
    }

    hptr<gpu::command_buffer> renderer::get_active_command_buffer()
    {
        if (m_currentCmdBuffer)
        {
            return m_currentCmdBuffer;
        }

        const gpu::result cmd = m_commandBufferPools.allocate_command_buffer();

        if (!cmd || !m_gpu->begin_command_buffer(*cmd))
        {
            return {};
        }

        m_currentCmdBuffer = *cmd;
        return *cmd;
    }

    resource_cache& renderer::get_resource_cache()
    {
        return m_platform->resourceCache;
    }

    renderer_platform& renderer::get_renderer_platform()
    {
        return *m_platform;
    }
}