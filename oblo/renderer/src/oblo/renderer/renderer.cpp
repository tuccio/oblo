#include <oblo/vulkan/renderer.hpp>

#include <oblo/core/string/string.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/gpu_queue_context.hpp>
#include <oblo/gpu/vulkan/vulkan_instance.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/draw/descriptor_set_pool.hpp>
#include <oblo/vulkan/draw/instance_data_type_registry.hpp>
#include <oblo/vulkan/renderer_module.hpp>
#include <oblo/vulkan/renderer_platform.hpp>

namespace oblo
{
    constexpr u32 StagingBufferSize{1u << 29};

    renderer::renderer() = default;
    renderer::~renderer() = default;

    bool renderer::init(const renderer::initializer& initializer)
    {
        // Only vulkan is currently supported
        auto* const vk = dynamic_cast<gpu::vk::vulkan_instance*>(m_gpu);

        if (!vk)
        {
            return false;
        }

        m_gpuQueueCtx = &initializer.queueContext;
        m_gpu = &m_gpuQueueCtx->get_instance();

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

        if (!m_frameGraph.init(*m_gpuQueueCtx))
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
        m_frameGraph.shutdown(*m_gpuQueueCtx);

        m_platform->passManager.shutdown(*m_gpuQueueCtx);

        m_platform->textureRegistry.shutdown();
        m_stagingBuffer.shutdown();
    }

    void renderer::begin_frame()
    {
        m_platform->resourceCache.update();

        m_stagingBuffer.notify_finished_frames(m_gpuQueueCtx->get_last_finished_submit());

        m_stagingBuffer.begin_frame(m_vkContext->get_submit_index());

        if (m_firstUpdate)
        {
            m_platform->textureRegistry.on_first_frame();

            m_firstUpdate = false;
        }
    }

    void renderer::end_frame()
    {
        const VkCommandBuffer commandBuffer = m_vkContext->get_active_command_buffer();

        m_platform->textureRegistry.flush_uploads(commandBuffer);

        m_platform->passManager.begin_frame(commandBuffer);

        m_frameGraph.build({
            .vkCtx = *m_vkContext,
            .stagingBuffer = m_stagingBuffer,
            .passManager = m_platform->passManager,
            .textureRegistry = m_platform->textureRegistry,
            .resourceCache = m_platform->resourceCache,
        });

        // Frame graph building might update the texture descriptors, so we update them after that
        m_platform->passManager.update_global_descriptor_sets();

        m_frameGraph.execute({
            .vkCtx = *m_vkContext,
            .stagingBuffer = m_stagingBuffer,
            .stringInterner = m_stringInterner,
            .passManager = m_platform->passManager,
            .textureRegistry = m_platform->textureRegistry,
        });

        m_platform->passManager.end_frame();
        m_stagingBuffer.end_frame();
    }

    const instance_data_type_registry& renderer::get_instance_data_type_registry() const
    {
        return *m_instanceDataTypeRegistry;
    }
}