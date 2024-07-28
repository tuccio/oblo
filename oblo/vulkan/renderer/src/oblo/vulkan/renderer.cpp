#include <oblo/vulkan/renderer.hpp>

#include <oblo/core/string/string.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/draw/descriptor_set_pool.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/renderer_context.hpp>
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
        m_drawRegistry.generate_raytracing_structures(frameAllocator, commandBuffer.get());

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

    namespace
    {
        VkPhysicalDeviceMeshShaderFeaturesEXT g_meshShaderFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
            .taskShader = true,
            .meshShader = true,
        };

        VkPhysicalDeviceVulkan12Features g_deviceVulkan12Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &g_meshShaderFeatures,
            .drawIndirectCount = true,
            .storageBuffer8BitAccess = true,
            .shaderInt8 = true,
            .descriptorBindingSampledImageUpdateAfterBind = true,
            .descriptorBindingPartiallyBound = true,
            .descriptorBindingVariableDescriptorCount = true,
            .runtimeDescriptorArray = true,
            .hostQueryReset = true,
            .timelineSemaphore = true,
            .bufferDeviceAddress = true,
        };

        VkPhysicalDeviceSynchronization2Features g_synchronizationFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            .pNext = &g_deviceVulkan12Features,
            .synchronization2 = true,
        };

        VkPhysicalDeviceRayQueryFeaturesKHR g_rtRayQueryFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
            .pNext = &g_synchronizationFeatures,
            .rayQuery = true,
        };

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR g_rtPipelineFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &g_rtRayQueryFeatures,
            .rayTracingPipeline = true,
        };

        VkPhysicalDeviceAccelerationStructureFeaturesKHR g_accelerationFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &g_rtPipelineFeatures,
            .accelerationStructure = true,
        };

        VkPhysicalDeviceShaderDrawParametersFeatures g_shaderDrawParameters{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
            .shaderDrawParameters = true,
        };

        constexpr const char* g_instanceExtensions[] = {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        };

        constexpr const char* g_deviceExtensions[] = {
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
            VK_EXT_MESH_SHADER_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, // This is only needed for debug printf
            VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,    // We need this for profiling with Tracy
            VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,         // We need this for profiling with Tracy
        };
    }

    required_features renderer::get_required_features()
    {
        return {
            .instanceExtensions = g_instanceExtensions,
            .deviceExtensions = g_deviceExtensions,
            .physicalDeviceFeatures =
                {
                    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                    .pNext = &g_shaderDrawParameters,
                    .features =
                        {
                            .multiDrawIndirect = true,
                            .shaderInt64 = true,
                        },
                },
            .deviceFeaturesChain = &g_accelerationFeatures,
        };
    }
}