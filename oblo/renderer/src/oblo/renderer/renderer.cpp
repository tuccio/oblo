#include <oblo/renderer/renderer.hpp>

#include <oblo/core/string/string.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/vulkan/vulkan_instance.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/renderer/draw/instance_data_type_registry.hpp>
#include <oblo/renderer/platform/renderer_platform.hpp>
#include <oblo/renderer/renderer_module.hpp>
#include <oblo/trace/profile.hpp>

namespace oblo
{
    constexpr u32 StagingBufferSize{1u << 30};

    struct renderer::used_command_buffer_pool
    {
        h32<gpu::command_buffer_pool> pool;
        u64 submitIndex;
    };

    renderer::renderer() = default;
    renderer::~renderer() = default;

    bool renderer::init(const renderer::initializer& initializer)
    {
        m_gpu = &initializer.gpu;

        // Only vulkan is currently supported
        auto* const vk = dynamic_cast<gpu::vk::vulkan_instance*>(m_gpu);

        if (!vk)
        {
            return false;
        }

        m_isRayTracingEnabled = initializer.isRayTracingEnabled;

        m_platform = allocate_unique<renderer_platform>();

        if (!m_stagingBuffer.init(*m_gpu, StagingBufferSize))
        {
            return false;
        }

        if (!m_platform->textureRegistry.init(*vk, m_stagingBuffer))
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

        m_platform->passManager.init(*vk, m_stringInterner, m_platform->textureRegistry, *m_instanceDataTypeRegistry);

        m_platform->passManager.set_raytracing_enabled(m_isRayTracingEnabled);

        const string_view includePaths[] = {"./vulkan/shaders/", "./imgui/shaders"};
        m_platform->passManager.set_system_include_paths(includePaths);

        m_platform->resourceCache.init(m_platform->textureRegistry);

        m_firstUpdate = true;

        return true;
    }

    void renderer::shutdown()
    {
        if (m_gpu)
        {
            m_frameGraph.shutdown(*m_gpu);

            if (m_currentCmdBufferPool)
            {
                m_gpu->destroy(m_currentCmdBufferPool);
            }

            for (const auto& used : m_usedPools)
            {
                m_gpu->destroy(used.pool);
            }
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

        m_stagingBuffer.begin_frame(m_gpu->get_submit_index());

        if (m_firstUpdate)
        {
            m_platform->textureRegistry.on_first_frame();

            m_firstUpdate = false;
        }

        m_gpu->begin_submit_tracking().assert_value();
    }

    hptr<gpu::command_buffer> renderer::end_frame()
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
            .commandBuffer = get_active_command_buffer(),
            .stagingBuffer = m_stagingBuffer,
        };

        m_frameGraph.execute(executeArgs);

        m_platform->passManager.end_frame();
        m_stagingBuffer.end_frame();

        return finalize_command_buffer_for_submission();
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

        if (!m_currentCmdBufferPool)
        {
            if (!m_usedPools.empty() && m_gpu->is_submit_done(m_usedPools.front().submitIndex))
            {
                m_currentCmdBufferPool = m_usedPools.front().pool;
                m_usedPools.pop_front();

                m_gpu->reset_command_buffer_pool(m_currentCmdBufferPool).assert_value();
            }
            else
            {
                m_currentCmdBufferPool = m_gpu
                                             ->create_command_buffer_pool({
                                                 .queue = m_gpu->get_universal_queue(),
                                                 .numCommandBuffers = 8,
                                             })
                                             .assert_value_or({});

                if (!m_currentCmdBufferPool)
                {
                    return {};
                }
            }
        }

        if (m_gpu->fetch_command_buffers(m_currentCmdBufferPool, {&m_currentCmdBuffer, 1u}))
        {
            if (!m_gpu->begin_command_buffer(m_currentCmdBuffer))
            {
                m_currentCmdBuffer = {};
            }
        }

        return m_currentCmdBuffer;
    }

    hptr<gpu::command_buffer> renderer::finalize_command_buffer_for_submission()
    {
        if (m_currentCmdBufferPool)
        {
            m_usedPools.emplace_back(m_currentCmdBufferPool, m_gpu->get_submit_index());
            m_currentCmdBufferPool = {};
        }

        const hptr<gpu::command_buffer> cmd = m_currentCmdBuffer;

        if (cmd)
        {
            m_gpu->end_command_buffer(cmd).assert_value();
            m_currentCmdBuffer = {};
        }

        return cmd;
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