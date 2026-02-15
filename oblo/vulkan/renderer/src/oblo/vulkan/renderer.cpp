#include <oblo/vulkan/renderer.hpp>

#include <oblo/core/string/string.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/draw/descriptor_set_pool.hpp>
#include <oblo/vulkan/draw/instance_data_type_registry.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/renderer_module.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    constexpr u32 StagingBufferSize{1u << 29};

    renderer::renderer() = default;
    renderer::~renderer() = default;

    bool renderer::init(const renderer::initializer& initializer)
    {
        m_vkContext = &initializer.vkContext;
        m_isRayTracingEnabled = initializer.isRayTracingEnabled;

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(m_vkContext->get_physical_device(), &properties);

        auto& gpuAllocator = m_vkContext->get_allocator();

        if (!m_stagingBuffer.init(gpuAllocator, StagingBufferSize, properties.limits))
        {
            return false;
        }

        if (!m_textureRegistry.init(*m_vkContext, m_stagingBuffer))
        {
            return false;
        }

        if (!m_frameGraph.init(*m_vkContext))
        {
            return false;
        }

        m_stringInterner.init(256);

        m_instanceDataTypeRegistry = allocate_unique<instance_data_type_registry>();
        m_instanceDataTypeRegistry->register_from_module();

        m_passManager.init(*m_vkContext, m_stringInterner, m_textureRegistry, *m_instanceDataTypeRegistry);

        m_passManager.set_raytracing_enabled(m_isRayTracingEnabled);

        const string_view includePaths[] = {"./vulkan/shaders/", "./imgui/shaders"};
        m_passManager.set_system_include_paths(includePaths);

        m_resourceCache.init(m_textureRegistry);

        m_firstUpdate = true;

        return true;
    }

    void renderer::shutdown()
    {
        m_frameGraph.shutdown(*m_vkContext);

        m_passManager.shutdown(*m_vkContext);

        m_textureRegistry.shutdown();
        m_stagingBuffer.shutdown();
    }

    void renderer::begin_frame()
    {
        m_resourceCache.update();

        m_stagingBuffer.notify_finished_frames(m_vkContext->get_last_finished_submit());

        m_stagingBuffer.begin_frame(m_vkContext->get_submit_index());

        if (m_firstUpdate)
        {
            m_textureRegistry.on_first_frame();

            m_firstUpdate = false;
        }
    }

    void renderer::end_frame()
    {
        const VkCommandBuffer commandBuffer = m_vkContext->get_active_command_buffer();

        m_textureRegistry.flush_uploads(commandBuffer);

        m_passManager.begin_frame(commandBuffer);

        m_frameGraph.build({
            .vkCtx = *m_vkContext,
            .stagingBuffer = m_stagingBuffer,
            .passManager = m_passManager,
            .textureRegistry = m_textureRegistry,
            .resourceCache = m_resourceCache,
        });

        // Frame graph building might update the texture descriptors, so we update them after that
        m_passManager.update_global_descriptor_sets();

        m_frameGraph.execute({
            .vkCtx = *m_vkContext,
            .stagingBuffer = m_stagingBuffer,
            .stringInterner = m_stringInterner,
            .passManager = m_passManager,
            .textureRegistry = m_textureRegistry,
        });

        m_passManager.end_frame();
        m_stagingBuffer.end_frame();
    }

    const instance_data_type_registry& renderer::get_instance_data_type_registry() const
    {
        return *m_instanceDataTypeRegistry;
    }
}