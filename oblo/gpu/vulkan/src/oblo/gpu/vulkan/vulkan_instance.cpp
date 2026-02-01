#include <oblo/gpu/vulkan/vulkan_instance.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/gpu/descriptors.hpp>
#include <oblo/gpu/vulkan/error.hpp>

namespace oblo::gpu
{
    namespace
    {
        constexpr h32<queue> universal_queue_id{1u};

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

    struct vulkan_instance::queue_impl
    {
        VkQueue queue;
        u32 familyIndex;
    };

    vulkan_instance::vulkan_instance() = default;

    vulkan_instance::~vulkan_instance()
    {
        shutdown();
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
        if (m_device)
        {
            vkDestroyDevice(m_device, nullptr);
            m_device = {};
        }

        if (m_instance)
        {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = {};
        }
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

    result<> vulkan_instance::finalize_init(const device_descriptor& deviceDescriptor, hptr<surface> presentSurface)
    {
        u32 physicalDevicesCount{0u};

        if (const VkResult r = vkEnumeratePhysicalDevices(m_instance, &physicalDevicesCount, nullptr); r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        if (physicalDevicesCount == 0u)
        {
            return error::undefined;
        }

        buffered_array<VkPhysicalDevice, 16> devices;
        devices.resize(physicalDevicesCount);

        if (const VkResult r = vkEnumeratePhysicalDevices(m_instance, &physicalDevicesCount, devices.data());
            r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        // TODO: It should actually search for the best GPU and check for API version, but we pick the first
        m_physicalDevice = devices[0];

        // Now find the queues
        constexpr float queuePriorities[] = {1.f};

        u32 queueFamiliesCount{0u};

        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamiliesCount, nullptr);

        buffered_array<VkQueueFamilyProperties, 32> queueProperties;
        queueProperties.resize(queueFamiliesCount);

        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamiliesCount, queueProperties.data());

        // For now we only search for a universal queue
        constexpr VkQueueFlags requiredQueueFlags =
            VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;

        const VkSurfaceKHR vkSurface = unwrap_handle<VkSurfaceKHR>(presentSurface);

        bool queueFound = false;
        u32 universalQueueFamilyIndex = ~0u;

        for (u32 i = 0; i < queueFamiliesCount; ++i)
        {
            if (const auto& properties = queueProperties[i];
                (properties.queueFlags & requiredQueueFlags) == requiredQueueFlags)
            {
                bool isSupported = true;

                if (vkSurface)
                {
                    VkBool32 presentSupport;
                    vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, vkSurface, &presentSupport);

                    isSupported = presentSupport == VK_TRUE;
                }

                if (isSupported)
                {
                    universalQueueFamilyIndex = i;
                    queueFound = true;
                    break;
                }
            }
        }

        if (!queueFound)
        {
            return error::undefined;
        }

        const VkDeviceQueueCreateInfo universalQueueInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0u,
            .queueFamilyIndex = universalQueueFamilyIndex,
            .queueCount = 1u,
            .pQueuePriorities = queuePriorities,
        };

        VkPhysicalDeviceFeatures2 g_physicalDeviceFeatures2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .features = g_physicalDeviceFeatures,
        };

        // Now we can create rest of the vulkan objects
        constexpr const char* requiredDeviceExtensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
            VK_EXT_MESH_SHADER_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, // This is needed for debug printf
            VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,    // We need this for profiling with Tracy
            VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,         // We need this for profiling with Tracy

        };

        // Ray-tracing extensions, we might want to disable them
        constexpr const char* rayTracingExtensions[] = {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        };

        constexpr auto totalExtensions = array_size(requiredDeviceExtensions) + array_size(rayTracingExtensions);
        buffered_array<const char*, totalExtensions> deviceExtensions;
        deviceExtensions.append(std::begin(requiredDeviceExtensions), std::end(requiredDeviceExtensions));

        if (deviceDescriptor.requireHardwareRaytracing)
        {
            deviceExtensions.append(std::begin(rayTracingExtensions), std::end(rayTracingExtensions));
            g_physicalDeviceFeatures2.pNext = &g_rtPipelineFeatures;
        }
        else
        {
            g_physicalDeviceFeatures2.pNext = &g_synchronizationFeatures;
        }

        const VkDeviceCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &g_physicalDeviceFeatures2,
            .queueCreateInfoCount = 1u,
            .pQueueCreateInfos = &universalQueueInfo,
            .enabledLayerCount = 0u,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = deviceExtensions.size32(),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = &g_physicalDeviceFeatures,
        };

        m_queues.push_back({
            .familyIndex = universalQueueFamilyIndex,
        });

        if (const VkResult r = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device); r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        for (auto& q : m_queues)
        {
            vkGetDeviceQueue(m_device, q.familyIndex, 0, &q.queue);
        }

        return no_error;
    }

    h32<queue> vulkan_instance::get_universal_queue()
    {
        return universal_queue_id;
    }

    result<h32<swapchain>> vulkan_instance::create_swapchain(const swapchain_descriptor& swapchain)
    {

        return error::undefined;
    }

    void vulkan_instance::destroy_swapchain(h32<swapchain> handle) {}

    result<h32<image>> vulkan_instance::acquire_swapchain_image(h32<swapchain> handle, h32<semaphore> waitSemaphore)
    {
        //
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

    result<> vulkan_instance::submit(h32<queue> queue, const queue_submit_descriptor& descriptor)
    {
        (void) queue;
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