#include <oblo/vulkan/renderer.hpp>

#include <oblo/core/string/string.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/draw/descriptor_set_pool.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/renderer_module.hpp>
#include <oblo/vulkan/required_features.hpp>
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
        m_isRayTracingEnabled = renderer_module::get().is_ray_tracing_enabled();

        if (!m_stagingBuffer.init(get_allocator(), StagingBufferSize))
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

        m_stringInterner.init(64);
        m_passManager.init(*m_vkContext, m_stringInterner, resourceManager.get(m_dummy), m_textureRegistry);
        m_passManager.set_raytracing_enabled(m_isRayTracingEnabled);

        const string includePaths[] = {"./vulkan/shaders/"};
        m_passManager.set_system_include_paths(includePaths);

        m_drawRegistry.init(*m_vkContext,
            m_stagingBuffer,
            m_stringInterner,
            initializer.entities,
            initializer.resources);

        m_firstUpdate = true;

        return true;
    }

    void renderer::shutdown()
    {
        auto& allocator = m_vkContext->get_allocator();
        auto& resourceManager = m_vkContext->get_resource_manager();

        m_drawRegistry.shutdown();

        m_frameGraph.shutdown(*m_vkContext);

        m_passManager.shutdown(*m_vkContext);

        resourceManager.destroy(allocator, m_dummy);

        m_textureRegistry.shutdown();
        m_stagingBuffer.shutdown();
    }

    void renderer::update(frame_allocator& frameAllocator)
    {
        OBLO_PROFILE_SCOPE();

        auto& commandBuffer = m_vkContext->get_active_command_buffer();

        m_stagingBuffer.notify_finished_frames(m_vkContext->get_last_finished_submit());

        m_stagingBuffer.begin_frame(m_vkContext->get_submit_index());

        if (m_firstUpdate)
        {
            m_textureRegistry.on_first_frame();

            // Not sure if we ever need to update it in other frames than the first
            const auto instanceDataDefines = m_drawRegistry.refresh_instance_data_defines(frameAllocator);
            m_passManager.update_instance_data_defines(instanceDataDefines);

            m_firstUpdate = false;
        }

        m_drawRegistry.flush_uploads(commandBuffer.get());
        m_textureRegistry.flush_uploads(commandBuffer.get());

        m_drawRegistry.generate_mesh_database(frameAllocator);
        m_drawRegistry.generate_draw_calls(frameAllocator, m_stagingBuffer);

        if (m_isRayTracingEnabled)
        {
            m_drawRegistry.generate_raytracing_structures(frameAllocator, commandBuffer.get());
        }

        m_frameGraph.build(*this);

        // Frame graph building might update the texture descriptors, so we begin the pass manager frame after that
        m_passManager.begin_frame(commandBuffer.get());

        m_frameGraph.execute(*this);

        m_passManager.end_frame();
        m_drawRegistry.end_frame();
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
}