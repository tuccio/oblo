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

        if (!m_stagingBuffer.init(get_allocator(), StagingBufferSize, properties.limits))
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

        auto& resourceManager = m_vkContext->get_resource_manager();

        m_dummy = resourceManager.create(get_allocator(),
            {
                .size = 16u,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .memoryUsage = memory_usage::gpu_only,
            });

        m_stringInterner.init(256);

        m_instanceDataTypeRegistry = allocate_unique<instance_data_type_registry>();
        m_instanceDataTypeRegistry->register_from_module();

        m_passManager.init(*m_vkContext,
            m_stringInterner,
            resourceManager.get(m_dummy),
            m_textureRegistry,
            *m_instanceDataTypeRegistry);

        m_passManager.set_raytracing_enabled(m_isRayTracingEnabled);

        const string_view includePaths[] = {"./vulkan/shaders/", "./imgui/shaders"};
        m_passManager.set_system_include_paths(includePaths);

        m_resourceCache.init(initializer.resources, m_textureRegistry);

        m_firstUpdate = true;

        return true;
    }

    void renderer::shutdown()
    {
        auto& allocator = m_vkContext->get_allocator();
        auto& resourceManager = m_vkContext->get_resource_manager();

        m_frameGraph.shutdown(*m_vkContext);

        m_passManager.shutdown(*m_vkContext);

        resourceManager.destroy(allocator, m_dummy);

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
        auto& commandBuffer = m_vkContext->get_active_command_buffer();

        m_textureRegistry.flush_uploads(commandBuffer.get());

        m_passManager.begin_frame(commandBuffer.get());

        m_frameGraph.build(*this);

        // Frame graph building might update the texture descriptors, so we update them after that
        m_passManager.update_global_descriptor_sets();

        m_frameGraph.execute(*this);

        m_passManager.end_frame();
        m_stagingBuffer.end_frame();
    }

    single_queue_engine& renderer::get_engine()
    {
        return m_vkContext->get_engine();
    }

    gpu_allocator& renderer::get_allocator()
    {
        return m_vkContext->get_allocator();
    }

    resource_manager& renderer::get_resource_manager()
    {
        return m_vkContext->get_resource_manager();
    }

    stateful_command_buffer& renderer::get_active_command_buffer()
    {
        return m_vkContext->get_active_command_buffer();
    }

    const instance_data_type_registry& renderer::get_instance_data_type_registry() const
    {
        return *m_instanceDataTypeRegistry;
    }
}