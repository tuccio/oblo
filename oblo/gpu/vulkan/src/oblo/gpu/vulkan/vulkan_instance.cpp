#include <oblo/gpu/vulkan/vulkan_instance.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/gpu/descriptors.hpp>
#include <oblo/gpu/vulkan/descriptor_set_pool.hpp>
#include <oblo/gpu/vulkan/error.hpp>
#include <oblo/gpu/vulkan/swapchain_wrapper.hpp>
#include <oblo/gpu/vulkan/utility/convert_enum.hpp>
#include <oblo/gpu/vulkan/utility/image_utils.hpp>

namespace oblo::gpu::vk
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

    struct vulkan_instance::buffer_impl : vk::allocated_buffer
    {
    };

    struct vulkan_instance::command_buffer_pool_impl
    {
        VkCommandPool vkCommandPool{};
        dynamic_array<VkCommandBuffer> commandBuffers;
        usize currentyUsedBuffers{};
    };

    struct vulkan_instance::image_impl : vk::allocated_image
    {
        VkImageView view;
    };

    struct vulkan_instance::queue_impl
    {
        VkQueue queue;
        u32 familyIndex;
        u64 submitIndex;
        descriptor_set_pool pipelineDescriptors;
        descriptor_set_pool bindlessTexturesDescriptors;
    };

    struct vulkan_instance::shader_module_impl
    {
        VkShaderModule vkShaderModule;
        dynamic_array<u32> spirv;
    };

    struct vulkan_instance::swapchain_impl
    {
        swapchain_wrapper<3u> swapchainWrapper;
        h32<image> images[3u];
        u32 acquiredImage{~0u};
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
        const VkResult result = gpu::vk::create_surface(nativeWindow, m_instance, nullptr, &vkSurface);
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

        if (const VkResult r = m_allocator.init(m_instance, m_physicalDevice, m_device); r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        return no_error;
    }

    h32<queue> vulkan_instance::get_universal_queue()
    {
        return universal_queue_id;
    }

    result<h32<swapchain>> vulkan_instance::create_swapchain(const swapchain_descriptor& descriptor)
    {
        auto&& [it, handle] = m_swapchains.emplace();

        const VkResult r = it->swapchainWrapper.create(m_allocator,
            m_physicalDevice,
            m_device,
            unwrap_handle<VkSurfaceKHR>(descriptor.surface),
            descriptor.width,
            descriptor.height,
            vk::convert_enum(descriptor.format));

        if (r != VK_SUCCESS)
        {
            m_swapchains.erase(handle);
            return translate_error(r);
        }

        for (u32 i = 0; i < it->swapchainWrapper.get_image_count(); ++i)
        {
            it->images[i] =
                register_image(it->swapchainWrapper.get_image(i), it->swapchainWrapper.get_image_view(i), nullptr);
        }

        return handle;
    }

    void vulkan_instance::destroy_swapchain(h32<swapchain> handle)
    {
        auto& sc = m_swapchains.at(handle);

        for (u32 i = 0; i < sc.swapchainWrapper.get_image_count(); ++i)
        {
            if (sc.images[i])
            {
                unregister_image(sc.images[i]);
                sc.images[i] = {};
            }
        }

        sc.swapchainWrapper.destroy(m_allocator, m_device);
        m_swapchains.erase(handle);
    }

    result<h32<fence>> vulkan_instance::create_fence(const fence_descriptor& descriptor)
    {
        VkFence vkFence;

        const VkFenceCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = descriptor.createSignaled ? VK_FENCE_CREATE_SIGNALED_BIT : VkFenceCreateFlags{},
        };

        const VkResult r = vkCreateFence(m_device, &info, m_allocator.get_allocation_callbacks(), &vkFence);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        return m_fences.emplace(vkFence).second;
    }

    void vulkan_instance::destroy_fence(h32<fence> handle)
    {
        vkDestroyFence(m_device, m_fences.at(handle), m_allocator.get_allocation_callbacks());
        m_fences.erase(handle);
    }

    result<> vulkan_instance::wait_for_fences(const std::span<const h32<fence>> fences)
    {
        buffered_array<VkFence, 8> vkFences;
        vkFences.reserve(fences.size());

        for (const h32 h : fences)
        {
            vkFences.emplace_back(m_fences.at(h));
        }

        constexpr VkBool32 waitAll = VK_TRUE;
        constexpr u64 timeout = UINT64_MAX;

        return translate_result(vkWaitForFences(m_device, vkFences.size32(), vkFences.data(), waitAll, timeout));
    }

    result<> vulkan_instance::reset_fences(const std::span<const h32<fence>> fences)
    {
        buffered_array<VkFence, 8> vkFences;
        vkFences.reserve(fences.size());

        for (const h32 h : fences)
        {
            vkFences.emplace_back(m_fences.at(h));
        }

        return translate_result(vkResetFences(m_device, vkFences.size32(), vkFences.data()));
    }

    result<h32<semaphore>> vulkan_instance::create_semaphore(const semaphore_descriptor& descriptor)
    {
        void* pNext{};
        VkSemaphoreTypeCreateInfo timelineTypeCreateInfo;

        if (descriptor.timeline)
        {
            timelineTypeCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .pNext = pNext,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue = descriptor.timelineInitialValue,
            };

            pNext = &timelineTypeCreateInfo;
        }

        const VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = pNext,
        };

        VkSemaphore semaphore;
        const VkResult r =
            vkCreateSemaphore(m_device, &semaphoreInfo, m_allocator.get_allocation_callbacks(), &semaphore);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        auto&& [it, handle] = m_semaphores.emplace(semaphore);
        return handle;
    }

    void vulkan_instance::destroy_semaphore(h32<semaphore> handle)
    {
        vkDestroySemaphore(m_device, m_semaphores.at(handle), m_allocator.get_allocation_callbacks());
        m_semaphores.erase(handle);
    }

    result<u64> vulkan_instance::read_timeline_semaphore(h32<semaphore> handle)
    {
        u64 value;
        return translate_error_or_value(vkGetSemaphoreCounterValue(m_device, m_semaphores.at(handle), &value), value);
    }

    result<h32<image>> vulkan_instance::acquire_swapchain_image(h32<swapchain> handle, h32<semaphore> semaphore)
    {
        auto& sc = m_swapchains.at(handle);
        const VkSemaphore vkSemaphore = semaphore ? m_semaphores.at(semaphore) : VK_NULL_HANDLE;

        u32 imageIndex;

        const VkResult acquireImageResult = vkAcquireNextImageKHR(m_device,
            sc.swapchainWrapper.get(),
            UINT64_MAX,
            vkSemaphore,
            VK_NULL_HANDLE,
            &imageIndex);

        if (acquireImageResult != VK_SUCCESS)
        {
            return translate_error(acquireImageResult);
        }

        sc.acquiredImage = imageIndex;
        return sc.images[imageIndex];
    }

    result<h32<command_buffer_pool>> vulkan_instance::create_command_buffer_pool(
        const command_buffer_pool_descriptor& descriptor)
    {
        const u32 queueFamilyIndex = get_queue(descriptor.queue).familyIndex;

        // We may want to support VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT too at some point.
        const VkCommandPoolCreateInfo commandPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0u,
            .queueFamilyIndex = queueFamilyIndex,
        };

        auto* const allocatorCallbacks = m_allocator.get_allocation_callbacks();

        VkCommandPool commandPool{};

        if (const VkResult r = vkCreateCommandPool(m_device, &commandPoolCreateInfo, allocatorCallbacks, &commandPool);
            r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        dynamic_array<VkCommandBuffer> commandBuffers;

        if (descriptor.numCommandBuffers > 0)
        {
            commandBuffers.resize(descriptor.numCommandBuffers);

            const VkCommandBufferAllocateInfo allocateInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = descriptor.numCommandBuffers,
            };

            const VkResult r = vkAllocateCommandBuffers(m_device, &allocateInfo, commandBuffers.data());

            if (r != VK_SUCCESS)
            {
                vkDestroyCommandPool(m_device, commandPool, allocatorCallbacks);
                return translate_error(r);
            }
        }

        auto&& [poolIt, handle] = m_commandBufferPools.emplace();
        poolIt->vkCommandPool = commandPool;
        poolIt->commandBuffers = std::move(commandBuffers);
        poolIt->currentyUsedBuffers = 0u;

        return handle;
    }

    void vulkan_instance::destroy_command_buffer_pool(h32<command_buffer_pool> commandBufferPool)
    {
        auto& poolImpl = m_commandBufferPools.at(commandBufferPool);
        vkDestroyCommandPool(m_device, poolImpl.vkCommandPool, m_allocator.get_allocation_callbacks());
        m_commandBufferPools.erase(commandBufferPool);
    }

    result<> vulkan_instance::reset_command_buffer_pool(h32<command_buffer_pool> commandBufferPool)
    {
        auto& poolImpl = m_commandBufferPools.at(commandBufferPool);
        return translate_result(vkResetCommandPool(m_device, poolImpl.vkCommandPool, 0u));
    }

    result<> vulkan_instance::fetch_command_buffers(h32<command_buffer_pool> pool,
        std::span<hptr<command_buffer>> commandBuffers)
    {
        auto& poolImpl = m_commandBufferPools.at(pool);

        const usize numRemainingBuffers = poolImpl.commandBuffers.size() - poolImpl.currentyUsedBuffers;

        if (commandBuffers.size() < numRemainingBuffers)
        {
            return error::not_enough_command_buffers;
        }

        const auto fetchedBuffers =
            std::span{poolImpl.commandBuffers}.subspan(poolImpl.currentyUsedBuffers, commandBuffers.size());

        OBLO_ASSERT(fetchedBuffers.size_bytes() == commandBuffers.size_bytes());
        std::memcpy(commandBuffers.data(), fetchedBuffers.data(), commandBuffers.size_bytes());

        return no_error;
    }

    result<> vulkan_instance::begin_command_buffer(hptr<command_buffer> commandBuffer)
    {
        const VkCommandBufferBeginInfo info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        return translate_result(vkBeginCommandBuffer(unwrap_handle<VkCommandBuffer>(commandBuffer), &info));
    }

    result<> vulkan_instance::end_command_buffer(hptr<command_buffer> commandBuffer)
    {
        return translate_result(vkEndCommandBuffer(unwrap_handle<VkCommandBuffer>(commandBuffer)));
    }

    result<h32<buffer>> vulkan_instance::create_buffer(const buffer_descriptor& descriptor)
    {
        vk::allocated_buffer allocatedBuffer;

        const VkResult r = m_allocator.create_buffer(
            {
                .size = descriptor.size,
                .usage = vk::convert_enum_flags(descriptor.usages),
                .memoryUsage = vk::allocated_memory_usage(descriptor.memoryUsage),
            },
            &allocatedBuffer);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        const auto [it, h] = m_buffers.emplace(allocatedBuffer);
        return h;
    }

    void vulkan_instance::destroy_buffer(h32<buffer> bufferHandle)
    {
        const auto& buffer = m_buffers.at(bufferHandle);

        if (buffer.buffer || buffer.allocation)
        {
            m_allocator.destroy(buffer);
        }

        m_buffers.erase(bufferHandle);
    }

    result<h32<image>> vulkan_instance::create_image(const image_descriptor& descriptor)
    {
        vk::allocated_image allocatedImage;

        const VkFormat vkFormat = vk::convert_enum(descriptor.format);

        const VkResult r = m_allocator.create_image(
            {
                .flags = 0u,
                .imageType = vk::convert_image_type(descriptor.type),
                .format = vkFormat,
                .extent = {descriptor.width, descriptor.height, descriptor.depth},
                .mipLevels = descriptor.mipLevels,
                .arrayLayers = descriptor.arrayLayers,
                .samples = vk::convert_enum(descriptor.samples),
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = vk::convert_enum_flags(descriptor.usages),
                .memoryUsage = vk::allocated_memory_usage(descriptor.memoryUsage),
            },
            &allocatedImage);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        const auto [it, h] = m_images.emplace(allocatedImage);

        const expected view = image_utils::create_image_view(m_device,
            allocatedImage.image,
            vk::convert_image_view_type(descriptor.type),
            vkFormat,
            m_allocator.get_allocation_callbacks());

        if (!view)
        {
            destroy_image(h);
            return view.error();
        }

        return h;
    }

    void vulkan_instance::destroy_image(h32<image> imageHandle)
    {
        const auto& image = m_images.at(imageHandle);

        if (image.view)
        {
            vkDestroyImageView(m_device, image.view, m_allocator.get_allocation_callbacks());
        }

        if (image.image || image.allocation)
        {
            m_allocator.destroy(image);
        }

        m_images.erase(imageHandle);
    }

    result<h32<shader_module>> vulkan_instance::create_shader_module(const shader_module_descriptor& descriptor)
    {
        if (descriptor.format != shader_module_format::spirv || descriptor.data.size_bytes() % 4 != 0)
        {
            return error::invalid_usage;
        }

        const usize numU32 = descriptor.data.size_bytes() / 4;

        // Copy the spirv because we can use spirv-cross on it for reflection purposes
        dynamic_array<u32> spirv;
        spirv.resize_default(numU32);

        std::memcpy(spirv.data(), descriptor.data.data(), numU32);

        const VkShaderModuleCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size_bytes(),
            .pCode = spirv.data(),
        };

        VkShaderModule shaderModule;
        const VkResult r = vkCreateShaderModule(m_device, &info, m_allocator.get_allocation_callbacks(), &shaderModule);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        const auto [it, handle] = m_shaderModules.emplace();
        it->vkShaderModule = shaderModule;
        it->spirv = std::move(spirv);
        return handle;
    }

    void vulkan_instance::destroy_shader_module(h32<shader_module> handle)
    {
        shader_module_impl& impl = m_shaderModules.at(handle);
        vkDestroyShaderModule(m_device, impl.vkShaderModule, m_allocator.get_allocation_callbacks());
        m_shaderModules.erase(handle);
    }

    result<h32<render_pipeline>> vulkan_instance::create_render_pipeline(const render_pipeline_descriptor& descriptor)
    {
        (void) descriptor;
        return error::undefined;
    }

    void vulkan_instance::destroy_render_pipeline(h32<render_pipeline> handle)
    {
        (void) handle;
    }

    result<> vulkan_instance::begin_render_pass(hptr<command_buffer> cmdBuffer, h32<render_pipeline> pipeline)
    {
        (void) cmdBuffer;
        (void) pipeline;
        return error::undefined;
    }

    void vulkan_instance::end_render_pass(hptr<command_buffer> cmdBuffer)
    {
        (void) cmdBuffer;
    }

    result<h32<bindless_image>> vulkan_instance::acquire_bindless(h32<image> optImage)
    {
        (void) optImage;
        return error::undefined;
    }
    result<h32<bindless_image>> vulkan_instance::replace_bindless(h32<bindless_image> slot, h32<image> optImage)
    {
        (void) slot;
        (void) optImage;
        return error::undefined;
    }

    void vulkan_instance::release_bindless(h32<bindless_image> slot)
    {
        (void) slot;
    }

    result<> vulkan_instance::submit(h32<queue> queue, const queue_submit_descriptor& descriptor)
    {
        buffered_array<VkSemaphore, 8> waitSemaphores;
        buffered_array<VkSemaphore, 8> signalSemaphores;
        buffered_array<VkPipelineStageFlags, 8> waitStages;

        if (!descriptor.waitSemaphores.empty())
        {
            // This is probably not generic enough for all use cases
            waitStages.assign(waitSemaphores.size(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            waitSemaphores.reserve(descriptor.waitSemaphores.size());

            for (const h32 h : descriptor.waitSemaphores)
            {
                waitSemaphores.emplace_back(m_semaphores.at(h));
            }
        }

        if (!descriptor.signalSemaphores.empty())
        {
            signalSemaphores.reserve(descriptor.signalSemaphores.size());

            for (const h32 h : descriptor.signalSemaphores)
            {
                signalSemaphores.emplace_back(m_semaphores.at(h));
            }
        }

        auto* const commandBuffers = reinterpret_cast<const VkCommandBuffer* const>(descriptor.commandBuffers.data());
        const u32 commandBufferCount = u32(descriptor.commandBuffers.size());

        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = waitSemaphores.size32(),
            .pWaitSemaphores = waitSemaphores.data(),
            .pWaitDstStageMask = waitStages.data(),
            .commandBufferCount = commandBufferCount,
            .pCommandBuffers = commandBuffers,
            .signalSemaphoreCount = signalSemaphores.size32(),
            .pSignalSemaphores = signalSemaphores.data(),
        };

        VkFence vkFence{};

        if (descriptor.signalFence)
        {
            vkFence = m_fences.at(descriptor.signalFence);
        }

        return translate_result(vkQueueSubmit(get_queue(queue).queue, 1, &submitInfo, vkFence));
    }

    result<> vulkan_instance::present(const present_descriptor& descriptor)
    {
        buffered_array<VkSemaphore, 8> waitSemaphores;
        buffered_array<VkSwapchainKHR, 8> acquiredSwapchains;
        buffered_array<u32, 8> acquiredImageIndices;

        if (!descriptor.waitSemaphores.empty())
        {
            waitSemaphores.reserve(descriptor.waitSemaphores.size());

            for (const h32 h : descriptor.waitSemaphores)
            {
                waitSemaphores.emplace_back(m_semaphores.at(h));
            }
        }

        if (!descriptor.swapchains.empty())
        {
            acquiredSwapchains.reserve(descriptor.swapchains.size());
            acquiredImageIndices.reserve(descriptor.swapchains.size());

            for (const h32 h : descriptor.swapchains)
            {
                auto& swapchainImpl = m_swapchains.at(h);
                acquiredSwapchains.emplace_back(swapchainImpl.swapchainWrapper.get());
                acquiredImageIndices.emplace_back(swapchainImpl.acquiredImage);
            }
        }

        const VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = waitSemaphores.size32(),
            .pWaitSemaphores = waitSemaphores.data(),
            .swapchainCount = acquiredSwapchains.size32(),
            .pSwapchains = acquiredSwapchains.data(),
            .pImageIndices = acquiredImageIndices.data(),
            .pResults = nullptr,
        };

        return translate_result(vkQueuePresentKHR(get_queue(universal_queue_id).queue, &presentInfo));
    }

    result<> vulkan_instance::wait_idle()
    {
        return translate_result(vkDeviceWaitIdle(m_device));
    }

    h32<image> vulkan_instance::register_image(VkImage image, VkImageView view, VmaAllocation allocation)
    {
        auto&& [img, handle] = m_images.emplace();

        *img = {
            {
                .image = image,
                .allocation = allocation,
            },
            view,
        };

        return handle;
    }

    void vulkan_instance::unregister_image(h32<image> image)
    {
        m_images.erase(image);
    }

    const vulkan_instance::queue_impl& vulkan_instance::get_queue(h32<queue> queue) const
    {
        OBLO_ASSERT(queue);
        return m_queues[queue.value - 1];
    }
}

#ifdef WIN32
    #include <Windows.h>

    #include <vulkan/vulkan_win32.h>

namespace oblo::gpu::vk
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
                .hinstance = GetModuleHandleA(nullptr),
                .hwnd = hwnd,
            };

            return vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, allocator, vkSurface);
        }
    }
}
#endif