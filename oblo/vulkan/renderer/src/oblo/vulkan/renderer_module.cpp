#include <oblo/vulkan/renderer_module.hpp>
#include <oblo/vulkan/required_features.hpp>

namespace oblo::vk
{
    namespace
    {
        // Device features
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

        // From here on it's ray-tracing stuff, conditionally disabled
        VkPhysicalDeviceAccelerationStructureFeaturesKHR g_accelerationFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &g_synchronizationFeatures,
            .accelerationStructure = true,
        };

        VkPhysicalDeviceRayQueryFeaturesKHR g_rtRayQueryFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
            .pNext = &g_accelerationFeatures,
            .rayQuery = true,
        };

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR g_rtPipelineFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &g_rtRayQueryFeatures,
            .rayTracingPipeline = true,
        };

        // Physical device features
        VkPhysicalDeviceShaderDrawParametersFeatures g_shaderDrawParameters{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
            .shaderDrawParameters = true,
        };

        constexpr const char* g_instanceExtensions[] = {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        };

        constexpr const char* g_rayTracingDeviceExtensions[] = {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        };

        // Hardcoded for now, it should be an option
        constexpr bool g_withRayTracing = true;

        renderer_module* g_instance = nullptr;
    }

    renderer_module& renderer_module::get()
    {
        return *g_instance;
    }

    bool renderer_module::startup(const module_initializer&)
    {
        OBLO_ASSERT(!g_instance);

        m_deviceFeaturesChain = g_withRayTracing ? static_cast<void*>(&g_rtPipelineFeatures)
                                                 : static_cast<void*>(&g_synchronizationFeatures);

        m_instanceExtensions.assign(std::begin(g_instanceExtensions), std::end(g_instanceExtensions));
        m_instanceExtensions.assign(std::begin(g_instanceExtensions), std::end(g_instanceExtensions));

        m_deviceExtensions = {
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
            VK_EXT_MESH_SHADER_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, // This is needed for debug printf
            VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,    // We need this for profiling with Tracy
            VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,         // We need this for profiling with Tracy
        };

        if (g_withRayTracing)
        {
            m_deviceExtensions.append(std::begin(g_rayTracingDeviceExtensions), std::end(g_rayTracingDeviceExtensions));
        }

        g_instance = this;

        return true;
    }

    void renderer_module::shutdown()
    {
        OBLO_ASSERT(g_instance == this);
        g_instance = nullptr;
    }

    required_features renderer_module::get_required_features()
    {
        return {
            .instanceExtensions = m_instanceExtensions,
            .deviceExtensions = m_deviceExtensions,
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
            .deviceFeaturesChain = m_deviceFeaturesChain,
        };
    }

    bool renderer_module::is_ray_tracing_enabled() const
    {
        return g_withRayTracing;
    }
}