#include <oblo/gpu/vulkan/vulkan_instance.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/gpu/descriptors/instance_descriptor.hpp>
#include <oblo/gpu/vulkan/error.hpp>

namespace oblo::gpu
{
    namespace
    {
        constexpr VkPhysicalDeviceFeatures g_physicalDeviceFeatures{
            .multiDrawIndirect = true,
            .samplerAnisotropy = true,
            .shaderInt64 = true,
            .shaderInt16 = true,
        };

        VkPhysicalDeviceDynamicRenderingFeatures g_dynamicRenderingFeature{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .dynamicRendering = VK_TRUE,
        };

        VkPhysicalDeviceMeshShaderFeaturesEXT g_meshShaderFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
            .pNext = &g_dynamicRenderingFeature,
            .taskShader = true,
            .meshShader = true,
        };

        VkPhysicalDeviceVulkan11Features g_deviceVulkan11Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .pNext = &g_meshShaderFeatures,
            .storageBuffer16BitAccess = true,
            .shaderDrawParameters = true,
        };

        VkPhysicalDeviceVulkan12Features g_deviceVulkan12Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &g_deviceVulkan11Features,
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

        constexpr const char* g_instanceExtensions[] = {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_KHR_SURFACE_EXTENSION_NAME,

#ifdef WIN32
            "VK_KHR_win32_surface",
#endif
        };

        [[nodiscard]] VkResult create_surface(hptr<native_window> wh,
            VkInstance instance,
            const VkAllocationCallbacks* allocator,
            VkSurfaceKHR* vkSurface);

        template <typename T, typename V>
        constexpr hptr<T> wrap_handle(V v)
        {
            return std::bit_cast<hptr<T>>(v);
        }

        template <typename V, typename T>
        constexpr V unwrap_handle(hptr<T> h)
        {
            return std::bit_cast<V>(h);
        }
    }

    result<> vulkan_instance::init(const instance_descriptor& descriptor)
    {
        if (m_instance)
        {
            return error::already_initialized;
        }

        const VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = descriptor.application,
            .applicationVersion = 0,
            .pEngineName = descriptor.engine,
            .engineVersion = 0,
            .apiVersion = VK_API_VERSION_1_3,
        };

        const VkInstanceCreateInfo instanceInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = 0u,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = u32(array_size(g_instanceExtensions)),
            .ppEnabledExtensionNames = g_instanceExtensions,
        };

        return translate_result(vkCreateInstance(&instanceInfo, nullptr, &m_instance));
    }

    void vulkan_instance::shutdown()
    {
        // TODO
    }

    result<hptr<surface>> vulkan_instance::create_surface(hptr<native_window> nativeWindow)
    {
        VkSurfaceKHR vkSurface{};
        const VkResult result = gpu::create_surface(nativeWindow, m_instance, nullptr, &vkSurface);
        return translate_error_or_value(result, wrap_handle<surface>(vkSurface));
    }

    void vulkan_instance::destroy_surface(hptr<surface> handle)
    {
        vkDestroySurfaceKHR(m_instance, unwrap_handle<VkSurfaceKHR>(handle), nullptr);
    }

    result<> vulkan_instance::create_device_and_queues(const device_descriptor& deviceDescriptors,
        std::span<const queue_descriptor> queueDescriptors,
        std::span<h32<queue>> outQueues)
    {
        (void) deviceDescriptors;
        (void) queueDescriptors;
        (void) outQueues;
        return error::undefined;
    }

    result<h32<swapchain>> vulkan_instance::create_swapchain(const swapchain_descriptor& swapchain)
    {
        (void) swapchain;
        return error::undefined;
    }

    result<h32<command_buffer_pool>> vulkan_instance::create_command_buffer_pool(
        const command_buffer_pool_descriptor& descriptor)
    {
        (void) descriptor;
        return error::undefined;
    }

    result<> vulkan_instance::fetch_command_buffers(h32<command_buffer_pool> pool,
        std::span<hptr<command_buffer>> commandBuffers)
    {
        (void) pool;
        (void) commandBuffers;
        return error::undefined;
    }

    result<h32<buffer>> vulkan_instance::create_buffer(const buffer_descriptor& descriptor)
    {
        (void) descriptor;
        return error::undefined;
    }
    result<h32<image>> vulkan_instance::create_image(const image_descriptor& descriptor)
    {
        (void) descriptor;
        return error::undefined;
    }

    result<> vulkan_instance::submit(const queue_submit_descriptor& descriptor)
    {
        (void) descriptor;
        return error::undefined;
    }
}

#ifdef WIN32
    #include <Windows.h>

    #include <vulkan/vulkan_win32.h>

namespace oblo::gpu
{
    namespace
    {
        VkResult create_surface(hptr<native_window> wh,
            VkInstance instance,
            const VkAllocationCallbacks* allocator,
            VkSurfaceKHR* vkSurface)
        {
            const HWND hwnd = std::bit_cast<HWND>(wh);

            const VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{
                .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                .hinstance = GetModuleHandle(nullptr),
                .hwnd = hwnd,
            };

            return vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, allocator, vkSurface);
        }
    }
}
#endif