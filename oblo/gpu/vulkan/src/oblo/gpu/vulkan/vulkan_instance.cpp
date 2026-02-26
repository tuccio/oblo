#include <oblo/gpu/vulkan/vulkan_instance.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/overload.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/vulkan/descriptor_set_pool.hpp>
#include <oblo/gpu/vulkan/error.hpp>
#include <oblo/gpu/vulkan/swapchain_wrapper.hpp>
#include <oblo/gpu/vulkan/utility/convert_enum.hpp>
#include <oblo/gpu/vulkan/utility/image_utils.hpp>
#include <oblo/gpu/vulkan/utility/pipeline_barrier.hpp>
#include <oblo/gpu/vulkan/utility/vk_type_conversions.hpp>

#define OBLO_VK_LOAD_FN(name) PFN_##name(vkGetInstanceProcAddr(m_instance, #name))
#define OBLO_VK_LOAD_FN_ASSIGN(loader, name) (loader.name = PFN_##name(vkGetInstanceProcAddr(m_instance, #name)))

namespace oblo::gpu::vk
{
    namespace
    {
        // We probably want to make this configurable instead
        constexpr u32 max_bindless_images = 2048;

        constexpr h32<queue> universal_queue_id{1u};

        constexpr flags descriptor_pool_general_kinds = resource_binding_kind::uniform |
            resource_binding_kind::storage_buffer | resource_binding_kind::storage_image;

        constexpr flags descriptor_pool_texture_kinds =
            resource_binding_kind::sampler | resource_binding_kind::sampled_image;

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

    template <typename T>
    void vulkan_instance::label_vulkan_object(T obj, const debug_label& label)
    {
        if (label.empty())
        {
#ifdef OBLO_DEBUG
            m_objLabeler.set_object_name(m_device, obj, "Unnamed object");
#endif

            return;
        }

        m_objLabeler.set_object_name(m_device, obj, label.get());
    }

    struct vulkan_instance::acceleration_structure_impl
    {
        VkAccelerationStructureKHR vkAccelerationStructure{};
    };

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
        image_descriptor descriptor;
    };

    struct vulkan_instance::image_pool_impl
    {
        VmaAllocation allocation;
        dynamic_array<h32<image>> images;
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
    };

    struct vulkan_instance::sampler_impl
    {
        VkSampler vkSampler;
    };

    struct vulkan_instance::swapchain_impl
    {
        swapchain_wrapper<3u> swapchainWrapper;
        h32<image> images[3u];
        u32 acquiredImage{~0u};
    };

    struct vulkan_instance::bind_group_layout_impl
    {
        VkDescriptorSetLayout descriptorSetLayout;
        debug_label debugLabel;
        flags<resource_binding_kind> resourceKinds;
    };

    // Pipelines

    namespace
    {
        struct pipeline_base
        {
            VkPipeline pipeline;
            VkPipelineLayout pipelineLayout;
            debug_label label;
        };

        void destroy_pipeline_base(const pipeline_base& p, VkDevice device, const VkAllocationCallbacks* allocationCbs)
        {
            if (p.pipelineLayout)
            {
                vkDestroyPipelineLayout(device, p.pipelineLayout, allocationCbs);
            }

            if (p.pipeline)
            {
                vkDestroyPipeline(device, p.pipeline, allocationCbs);
            }
        }
    }

    struct vulkan_instance::graphics_pipeline_impl final : pipeline_base
    {
    };

    struct vulkan_instance::compute_pipeline_impl final : pipeline_base
    {
    };

    struct vulkan_instance::raytracing_pipeline_impl final : pipeline_base
    {
        gpu::vk::allocated_buffer shaderBindingTable{};

        VkStridedDeviceAddressRegionKHR rayGen{};
        VkStridedDeviceAddressRegionKHR hit{};
        VkStridedDeviceAddressRegionKHR miss{};
        VkStridedDeviceAddressRegionKHR callable{};
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

        const VkResult instanceResult = vkCreateInstance(&instanceInfo, nullptr, &m_instance);

        if (instanceResult != VK_SUCCESS)
        {
            translate_error(instanceResult);
        }

        // Load dynamic functions
        OBLO_VK_LOAD_FN_ASSIGN(m_objLabeler, vkSetDebugUtilsObjectNameEXT);

        OBLO_VK_LOAD_FN_ASSIGN(m_cmdLabeler, vkCmdBeginDebugUtilsLabelEXT);
        OBLO_VK_LOAD_FN_ASSIGN(m_cmdLabeler, vkCmdEndDebugUtilsLabelEXT);

        OBLO_VK_LOAD_FN_ASSIGN(m_loadedFunctions, vkCmdDrawMeshTasksEXT);
        OBLO_VK_LOAD_FN_ASSIGN(m_loadedFunctions, vkCmdDrawMeshTasksIndirectEXT);
        OBLO_VK_LOAD_FN_ASSIGN(m_loadedFunctions, vkCmdDrawMeshTasksIndirectCountEXT);
        OBLO_VK_LOAD_FN_ASSIGN(m_loadedFunctions, vkCreateAccelerationStructureKHR);
        OBLO_VK_LOAD_FN_ASSIGN(m_loadedFunctions, vkDestroyAccelerationStructureKHR);
        OBLO_VK_LOAD_FN_ASSIGN(m_loadedFunctions, vkGetAccelerationStructureBuildSizesKHR);
        OBLO_VK_LOAD_FN_ASSIGN(m_loadedFunctions, vkCmdBuildAccelerationStructuresKHR);
        OBLO_VK_LOAD_FN_ASSIGN(m_loadedFunctions, vkGetAccelerationStructureDeviceAddressKHR);
        OBLO_VK_LOAD_FN_ASSIGN(m_loadedFunctions, vkCreateRayTracingPipelinesKHR);
        OBLO_VK_LOAD_FN_ASSIGN(m_loadedFunctions, vkGetRayTracingShaderGroupHandlesKHR);
        OBLO_VK_LOAD_FN_ASSIGN(m_loadedFunctions, vkCmdTraceRaysKHR);

        return no_error;
    }

    void vulkan_instance::shutdown()
    {
        shutdown_tracked_queue_context();

        if (m_dummySampler)
        {
            destroy(m_dummySampler);
        }

        m_perFrameSetPool.shutdown();
        m_allocator.shutdown();

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

    void vulkan_instance::destroy(hptr<surface> handle)
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

        // Cache the properties so users can query them on demand
        m_raytracingProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        };

        m_subgroupProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,
            .pNext = &m_raytracingProperties,
        };

        m_physicalDeviceProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &m_subgroupProperties,
        };

        vkGetPhysicalDeviceProperties2(m_physicalDevice, &m_physicalDeviceProperties);

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

        // Those are currently all per-frame sets, we reset them once, we don't have a concept of long living-sets
        // currently

        m_perFrameSetPool.init(m_device, m_allocator.get_allocation_callbacks());

        m_perFrameSetPool.add_pool_kind(descriptor_pool_general_kinds,
            128u,
            0u,
            make_span_initializer<VkDescriptorPoolSize>({
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64},
            }));

        /// @see descriptor_pool_texture-kinds, this one allows bindless descriptors for textures
        m_perFrameSetPool.add_pool_kind(descriptor_pool_texture_kinds,
            2u, // This is just because we only need one per each type, but it should be revisited
            VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
            make_span_initializer<VkDescriptorPoolSize>({
                {VK_DESCRIPTOR_TYPE_SAMPLER, 32}, // We might want to expose or let users configure these limits
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, max_bindless_images},
            }));

        m_bindlessImages.resize(max_bindless_images);

        return init_tracked_queue_context();
    }

    h32<queue> vulkan_instance::get_universal_queue()
    {
        return universal_queue_id;
    }

    device_info vulkan_instance::get_device_info()
    {
        const auto& limits = m_physicalDeviceProperties.properties.limits;

        return {
            .subgroupSize = m_subgroupProperties.subgroupSize,
            .minUniformBufferOffsetAlignment = limits.minUniformBufferOffsetAlignment,
            .minStorageBufferOffsetAlignment = limits.minStorageBufferOffsetAlignment,
            .optimalBufferCopyOffsetAlignment = limits.optimalBufferCopyOffsetAlignment,
            .optimalBufferCopyRowPitchAlignment = limits.optimalBufferCopyRowPitchAlignment,
        };
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

        const image_descriptor imageDesc{
            .format = descriptor.format,
            .width = descriptor.width,
            .height = descriptor.height,
            .depth = 1,
            .mipLevels = 1,
            .arrayLayers = 1,
            .type = image_type::plain_2d,
            .samples = samples_count::one,
            .memoryUsage = memory_usage::gpu_only,
            .usages = {},
        };

        for (u32 i = 0; i < it->swapchainWrapper.get_image_count(); ++i)
        {
            it->images[i] = register_image(it->swapchainWrapper.get_image(i),
                it->swapchainWrapper.get_image_view(i),
                nullptr,
                imageDesc);
        }

        return handle;
    }

    void vulkan_instance::destroy(h32<swapchain> handle)
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

        label_vulkan_object(vkFence, descriptor.debugLabel);

        return m_fences.emplace(vkFence).second;
    }

    void vulkan_instance::destroy(h32<fence> handle)
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

        label_vulkan_object(semaphore, descriptor.debugLabel);

        auto&& [it, handle] = m_semaphores.emplace(semaphore);
        return handle;
    }

    void vulkan_instance::destroy(h32<semaphore> handle)
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

    result<h32<bind_group_layout>> vulkan_instance::create_bind_group_layout(
        const bind_group_layout_descriptor& descriptor)
    {
        buffered_array<VkDescriptorSetLayoutBinding, 32> bindings;
        bindings.reserve(descriptor.bindings.size());

        buffered_array<VkSampler, 32> samplers;

        flags<resource_binding_kind> bindingKinds{};

        for (const auto& binding : descriptor.bindings)
        {
            const bool hasSamplers = !binding.immutableSamplers.empty();

            if (hasSamplers)
            {
                const usize samplersCount = binding.immutableSamplers.size();
                samplers.reserve(samplersCount);

                for (usize i = 0; i < samplersCount; ++i)
                {
                    const h32 samplerHandle = binding.immutableSamplers[i];
                    const VkSampler vkSampler = m_samplers.at(samplerHandle).vkSampler;
                    samplers.emplace_back(vkSampler);
                }
            }

            OBLO_ASSERT(binding.count > 0, "Probably forgot to set the count");

            bindings.push_back({
                .binding = binding.binding,
                .descriptorType = convert_enum(binding.bindingKind),
                .descriptorCount = binding.count,
                .stageFlags = convert_enum_flags(binding.shaderStages),
                .pImmutableSamplers = hasSamplers ? samplers.data() : nullptr,
            });

            bindingKinds |= binding.bindingKind;
        }

        // These are needed for bindless textures
        constexpr VkDescriptorBindingFlags bindlessFlags[] = {
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
                VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT,
        };

        VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extendedInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
            .bindingCount = array_size32(bindlessFlags),
            .pBindingFlags = bindlessFlags,
        };

        void* pAcquireNext{};
        VkDescriptorSetLayoutCreateFlags createFlags{};
        flags<resource_binding_kind> resourceKinds{};

        if (descriptor_pool_general_kinds.contains_all(bindingKinds))
        {
            resourceKinds = descriptor_pool_general_kinds;
        }
        else if (descriptor_pool_texture_kinds.contains_all(bindingKinds))
        {
            createFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
            pAcquireNext = &extendedInfo;

            resourceKinds = descriptor_pool_texture_kinds;
        }
        else
        {
            // We don't have a pool for this combination, we could decide to create one
            return error::invalid_usage;
        }

        const VkDescriptorSetLayoutCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = pAcquireNext,
            .flags = createFlags,
            .bindingCount = bindings.size32(),
            .pBindings = bindings.data(),
        };

        VkDescriptorSetLayout vkLayout;
        const VkResult r =
            vkCreateDescriptorSetLayout(m_device, &createInfo, m_allocator.get_allocation_callbacks(), &vkLayout);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        label_vulkan_object(vkLayout, descriptor.debugLabel);

        auto [it, handle] = m_bindGroupLayouts.emplace();

        it->descriptorSetLayout = vkLayout;
        it->resourceKinds = resourceKinds;
        it->debugLabel = descriptor.debugLabel;

        return handle;
    }

    void vulkan_instance::destroy(h32<bind_group_layout> handle)
    {
        auto& layout = m_bindGroupLayouts.at(handle);
        vkDestroyDescriptorSetLayout(m_device, layout.descriptorSetLayout, m_allocator.get_allocation_callbacks());
        m_bindGroupLayouts.erase(handle);
    }

    result<hptr<bind_group>> vulkan_instance::acquire_transient_bind_group(h32<bind_group_layout> handle,
        std::span<const bind_group_data> data)
    {
        const bind_group_layout_impl& layoutImpl = m_bindGroupLayouts.at(handle);
        const VkDescriptorSetLayout layout = layoutImpl.descriptorSetLayout;

        const result r =
            m_perFrameSetPool.acquire(layoutImpl.resourceKinds, get_last_finished_submit(), layout, nullptr);

        if (!r)
        {
            return r.error();
        }

        label_vulkan_object(*r, layoutImpl.debugLabel);

        initialize_descriptor_set(*r, data);

        return wrap_handle<bind_group>(*r);
    }

    u32 vulkan_instance::get_max_bindless_images() const
    {
        return max_bindless_images;
    }

    result<> vulkan_instance::set_bindless_images(std::span<const bindless_image_descriptor> images, u32 first)
    {
        if (first + images.size() > m_bindlessImages.size())
        {
            return error::invalid_usage;
        }

        const VkSampler anySampler = get_or_create_dummy_sampler();

        u32 currentIdx = first;

        for (const auto& desc : images)
        {
            const auto& imageImpl = m_images.at(desc.image);
            const auto layout = deduce_layout(desc.state);

            m_bindlessImages[currentIdx] = {
                .sampler = anySampler,
                .imageView = imageImpl.view,
                .imageLayout = layout,
            };

            ++currentIdx;
        }

        return no_error;
    }

    result<hptr<bind_group>> vulkan_instance::acquire_transient_bindless_images_bind_group(
        h32<bind_group_layout> handle, u32 binding, u32 count)
    {
        const bind_group_layout_impl& layoutImpl = m_bindGroupLayouts.at(handle);

        VkDescriptorSetVariableDescriptorCountAllocateInfoEXT countInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
            .descriptorSetCount = 1,
            .pDescriptorCounts = &count,
        };

        const result<VkDescriptorSet> r = m_perFrameSetPool.acquire(layoutImpl.resourceKinds,
            get_last_finished_submit(),
            layoutImpl.descriptorSetLayout,
            &countInfo);

        if (!r)
        {
            return r.error();
        }

        label_vulkan_object(*r, layoutImpl.debugLabel);

        const VkWriteDescriptorSet descriptorSetWrites[] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = *r,
                .dstBinding = binding,
                .dstArrayElement = 0,
                .descriptorCount = count,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .pImageInfo = m_bindlessImages.data(),
            },
        };

        vkUpdateDescriptorSets(m_device, array_size32(descriptorSetWrites), descriptorSetWrites, 0, nullptr);

        return wrap_handle<bind_group>(*r);
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

    void vulkan_instance::destroy(h32<command_buffer_pool> commandBufferPool)
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

        if (numRemainingBuffers < commandBuffers.size())
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

        vk::allocated_buffer_initializer initializer{
            .size = descriptor.size,
            .usage = vk::convert_enum_flags(descriptor.usages),
            .debugLabel = descriptor.debugLabel,
        };

        descriptor.memoryProperties.visit(overload{
            [&initializer](memory_usage usage) -> void { initializer.memoryUsage = vk::allocated_memory_usage(usage); },
            [&initializer](flags<memory_requirement> requirements) -> void
            { initializer.requiredFlags = vk::convert_enum_flags(requirements); },
        });

        const VkResult r = m_allocator.create_buffer(initializer, &allocatedBuffer);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        label_vulkan_object(allocatedBuffer.buffer, descriptor.debugLabel);

        const auto [it, h] = m_buffers.emplace(allocatedBuffer);
        return h;
    }

    void vulkan_instance::destroy(h32<buffer> bufferHandle)
    {
        const auto& buffer = m_buffers.at(bufferHandle);

        if (buffer.buffer || buffer.allocation)
        {
            m_allocator.destroy(buffer);
        }

        m_buffers.erase(bufferHandle);
    }

    h64<device_address> vulkan_instance::get_device_address(h32<buffer> bufferHandle)
    {
        const VkBufferDeviceAddressInfo info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = m_buffers.at(bufferHandle).buffer,
        };

        const VkDeviceAddress address = vkGetBufferDeviceAddress(m_device, &info);
        return {address};
    }

    h64<device_address> vulkan_instance::get_device_address(buffer_range bufferWithOffset)
    {
        const h64 address = get_device_address(bufferWithOffset.buffer);
        return offset_device_address(address, bufferWithOffset.offset);
    }

    result<h32<acceleration_structure>> vulkan_instance::create_acceleration_structure(
        const acceleration_structure_descriptor& descriptor)
    {
        // TODO
        (void) descriptor;
        return error::invalid_usage;
    }

    void vulkan_instance::destroy(h32<acceleration_structure> handle)
    {
        auto& asImpl = m_accelerationStructures.at(handle);
        m_loadedFunctions.vkDestroyAccelerationStructureKHR(m_device,
            asImpl.vkAccelerationStructure,
            m_allocator.get_allocation_callbacks());
        m_accelerationStructures.erase(handle);
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
                .debugLabel = descriptor.debugLabel,
            },
            &allocatedImage);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        label_vulkan_object(allocatedImage.image, descriptor.debugLabel);

        const auto [it, h] = m_images.emplace(allocatedImage);

        const expected view = image_utils::create_image_view(m_device,
            allocatedImage.image,
            vk::convert_image_view_type(descriptor.type),
            vkFormat,
            m_allocator.get_allocation_callbacks());

        it->descriptor = descriptor;

        if (!view)
        {
            destroy(h);
            return view.error();
        }

        it->view = *view;

        label_vulkan_object(*view, descriptor.debugLabel);

        return h;
    }

    void vulkan_instance::destroy(h32<image> imageHandle)
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

    image_descriptor vulkan_instance::get_image_descriptor(h32<image> imageHandle)
    {
        return m_images.at(imageHandle).descriptor;
    }

    result<h32<image_pool>> vulkan_instance::create_image_pool(std::span<const image_descriptor> descriptors,
        std::span<h32<image>> images)
    {
        if (descriptors.size() != images.size() || descriptors.empty())
        {
            return error::invalid_usage;
        }

        VkMemoryRequirements newRequirements{.memoryTypeBits = ~u32{}};

        const auto allocationCbs = m_allocator.get_allocation_callbacks();

        struct pooled_image_info
        {
            VkImage image;
            VkDeviceSize size;
        };

        buffered_array<pooled_image_info, 64> pooledTextures;
        pooledTextures.resize_default(descriptors.size());

        VmaAllocation allocation{};

        const auto cleanup = [&](usize count)
        {
            for (usize i = 0; i < count; ++i)
            {
                vkDestroyImage(m_device, pooledTextures[i].image, allocationCbs);
            }

            if (allocation)
            {
                m_allocator.destroy_memory(allocation);
            }
        };

        for (usize descriptorIdx = 0; descriptorIdx < descriptors.size(); ++descriptorIdx)
        {
            const image_descriptor& descriptor = descriptors[descriptorIdx];
            OBLO_ASSERT(descriptor.memoryUsage == gpu::memory_usage::gpu_only);

            const VkImageCreateInfo imageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .flags = 0u,
                .imageType = convert_image_type(descriptor.type),
                .format = convert_enum(descriptor.format),
                .extent = {descriptor.width, descriptor.height, descriptor.depth},
                .mipLevels = descriptor.mipLevels,
                .arrayLayers = descriptor.arrayLayers,
                .samples = convert_enum(descriptor.samples),
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = convert_enum_flags(descriptor.usages),
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            };

            VkImage image{};
            const VkResult result = vkCreateImage(m_device, &imageCreateInfo, allocationCbs, &image);

            if (result != VK_SUCCESS)
            {
                cleanup(descriptorIdx);

                return translate_error(result);
            }

            VkMemoryRequirements requirements;
            vkGetImageMemoryRequirements(m_device, image, &requirements);

            pooledTextures[descriptorIdx] = {
                .image = image,
                .size = requirements.size,
            };

            newRequirements.alignment = max(newRequirements.alignment, requirements.alignment);
            newRequirements.size += requirements.size;
            newRequirements.memoryTypeBits &= requirements.memoryTypeBits;
        }

        // We should maybe constrain memory type bits to use something that uses the bigger heap?
        // https://stackoverflow.com/questions/73243399/vma-how-to-tell-the-library-to-use-the-bigger-of-2-heaps
        OBLO_ASSERT(newRequirements.memoryTypeBits != 0);

        if (newRequirements.size == 0)
        {
            // Nothing to do?
            return error::invalid_usage;
        }

        // Add space for alignment
        newRequirements.size += (newRequirements.alignment - 1) * descriptors.size();

        allocation =
            m_allocator.create_memory(newRequirements, gpu::vk::allocated_memory_usage::gpu_only, "gpu::image_pool");

        VkDeviceSize offset{0};

        for (usize descriptorIdx = 0; descriptorIdx < descriptors.size(); ++descriptorIdx)
        {
            const pooled_image_info& t = pooledTextures[descriptorIdx];
            const VkResult result = m_allocator.bind_image_memory(t.image, allocation, offset);

            if (result != VK_SUCCESS)
            {
                cleanup(descriptors.size());
                return translate_error(result);
            }

            offset += t.size + t.size % newRequirements.alignment;

            const auto& descriptor = descriptors[descriptorIdx];

            const expected imageView = gpu::vk::image_utils::create_image_view_2d(m_device,
                t.image,
                convert_enum(descriptor.format),
                allocationCbs);

            if (!imageView)
            {
                cleanup(descriptors.size());

                return imageView.error();
            }

            label_vulkan_object(*imageView, descriptor.debugLabel);

            const auto [it, handle] = m_images.emplace();
            it->image = t.image;
            it->view = *imageView;

            images[descriptorIdx] = handle;
        }

        const auto [it, handle] = m_imagePools.emplace();
        it->allocation = allocation;
        it->images.assign(images.begin(), images.end());

        return handle;
    }

    void vulkan_instance::destroy(h32<image_pool> imagePoolHandle)
    {
        const image_pool_impl& pool = m_imagePools.at(imagePoolHandle);

        for (const h32 image : pool.images)
        {
            destroy(image);
        }

        m_allocator.destroy_memory(pool.allocation);
        m_imagePools.erase(imagePoolHandle);
    }

    result<h32<shader_module>> vulkan_instance::create_shader_module(const shader_module_descriptor& descriptor)
    {
        if (descriptor.format != shader_module_format::spirv)
        {
            return error::invalid_usage;
        }

        if (descriptor.data.size_bytes() % sizeof(u32) != 0 || uintptr(descriptor.data.data()) % sizeof(u32) != 0)
        {
            return error::invalid_usage;
        }

        const auto spirv = std::span{
            reinterpret_cast<const u32*>(descriptor.data.data()),
            descriptor.data.size_bytes() / sizeof(u32),
        };

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
        return handle;
    }

    void vulkan_instance::destroy(h32<shader_module> handle)
    {
        shader_module_impl& impl = m_shaderModules.at(handle);
        vkDestroyShaderModule(m_device, impl.vkShaderModule, m_allocator.get_allocation_callbacks());
        m_shaderModules.erase(handle);
    }

    result<h32<sampler>> vulkan_instance::create_sampler(const sampler_descriptor& descriptor)
    {
        const VkSamplerCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = convert_enum(descriptor.magFilter),
            .minFilter = convert_enum(descriptor.minFilter),
            .mipmapMode = convert_enum(descriptor.mipmapMode),
            .addressModeU = convert_enum(descriptor.addressModeU),
            .addressModeV = convert_enum(descriptor.addressModeV),
            .addressModeW = convert_enum(descriptor.addressModeW),
            .mipLodBias = descriptor.mipLodBias,
            .anisotropyEnable = descriptor.anisotropyEnable ? VK_TRUE : VK_FALSE,
            .maxAnisotropy = descriptor.maxAnisotropy,
            .compareEnable = descriptor.compareEnable,
            .compareOp = convert_enum(descriptor.compareOp),
            .minLod = descriptor.minLod,
            .maxLod = descriptor.maxLod,
            .borderColor = convert_enum(descriptor.borderColor),
            .unnormalizedCoordinates = descriptor.unnormalizedCoordinates,
        };

        VkSampler vkSampler;
        const VkResult r = vkCreateSampler(m_device, &info, m_allocator.get_allocation_callbacks(), &vkSampler);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        const auto [it, handle] = m_samplers.emplace();
        it->vkSampler = vkSampler;
        label_vulkan_object(vkSampler, descriptor.debugLabel);
        return handle;
    }

    void vulkan_instance::destroy(h32<sampler> handle)
    {
        sampler_impl& impl = m_samplers.at(handle);
        vkDestroySampler(m_device, impl.vkSampler, m_allocator.get_allocation_callbacks());
        m_samplers.erase(handle);
    }

    result<h32<graphics_pipeline>> vulkan_instance::create_graphics_pipeline(
        const graphics_pipeline_descriptor& descriptor)
    {
        const auto [newPipeline, handle] = m_renderPipelines.emplace();
        newPipeline->label = descriptor.debugLabel;

        auto cleanup = finally_if_not_cancelled([this, handle] { destroy(handle); });

        const expected pipelineLayoutRes = create_pipeline_layout(descriptor.pushConstants,
            descriptor.bindGroupLayouts,
            &newPipeline->pipelineLayout,
            descriptor.debugLabel);

        if (!pipelineLayoutRes)
        {
            return pipelineLayoutRes.error();
        }

        buffered_array<VkPipelineShaderStageCreateInfo, 4> stageCreateInfo;

        for (const auto& stage : descriptor.stages)
        {
            stageCreateInfo.push_back({
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = convert_enum(stage.stage),
                .module = unwrap_shader_module(stage.shaderModule),
                .pName = stage.entryFunction,
            });
        }

        static_assert(sizeof(VkFormat) == sizeof(image_format), "We rely on this when reinterpret casting");

        const u32 numAttachments = u32(descriptor.renderTargets.colorAttachmentFormats.size());

        const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = numAttachments,
            .pColorAttachmentFormats =
                reinterpret_cast<const VkFormat*>(descriptor.renderTargets.colorAttachmentFormats.data()),
            .depthAttachmentFormat = convert_to_vk(descriptor.renderTargets.depthFormat),
            .stencilAttachmentFormat = convert_to_vk(descriptor.renderTargets.stencilFormat),
        };

        const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = convert_to_vk(descriptor.primitiveTopology),
            .primitiveRestartEnable = VK_FALSE,
        };

        buffered_array<VkVertexInputBindingDescription, 4> vertexInputBindingDescs;
        vertexInputBindingDescs.reserve(descriptor.vertexInputBindings.size());

        for (auto& binding : descriptor.vertexInputBindings)
        {
            vertexInputBindingDescs.push_back({
                .binding = binding.binding,
                .stride = binding.stride,
                .inputRate = convert_enum(binding.inputRate),
            });
        }

        buffered_array<VkVertexInputAttributeDescription, 4> vertexInputAttributes;
        vertexInputBindingDescs.reserve(descriptor.vertexInputAttributes.size());

        for (const auto& attribute : descriptor.vertexInputAttributes)
        {
            vertexInputAttributes.push_back({
                .location = attribute.location,
                .binding = attribute.binding,
                .format = convert_enum(attribute.format),
            });
        }

        const VkPipelineVertexInputStateCreateInfo vertexBufferInputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = vertexInputBindingDescs.size32(),
            .pVertexBindingDescriptions = vertexInputBindingDescs.data(),
            .vertexAttributeDescriptionCount = vertexInputAttributes.size32(),
            .pVertexAttributeDescriptions = vertexInputAttributes.data(),
        };

        constexpr VkPipelineViewportStateCreateInfo viewportState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        const VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .flags = convert_to_vk(descriptor.rasterizationState.flags),
            .depthClampEnable = descriptor.rasterizationState.depthClampEnable,
            .rasterizerDiscardEnable = descriptor.rasterizationState.rasterizerDiscardEnable,
            .polygonMode = convert_to_vk(descriptor.rasterizationState.polygonMode),
            .cullMode = convert_to_vk(descriptor.rasterizationState.cullMode),
            .frontFace = convert_to_vk(descriptor.rasterizationState.frontFace),
            .depthBiasEnable = descriptor.rasterizationState.depthBiasEnable,
            .depthBiasConstantFactor = descriptor.rasterizationState.depthBiasConstantFactor,
            .depthBiasClamp = descriptor.rasterizationState.depthBiasClamp,
            .depthBiasSlopeFactor = descriptor.rasterizationState.depthBiasSlopeFactor,
            .lineWidth = descriptor.rasterizationState.lineWidth,

        };

        const VkPipelineMultisampleStateCreateInfo multisampling{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .minSampleShading = 1.f,
        };

        buffered_array<VkPipelineColorBlendAttachmentState, 8> colorBlendAttachments;
        colorBlendAttachments.reserve(descriptor.renderTargets.blendStates.size());

        for (auto& attachment : descriptor.renderTargets.blendStates)
        {
            colorBlendAttachments.push_back({
                .blendEnable = attachment.enable,
                .srcColorBlendFactor = convert_to_vk(attachment.srcColorBlendFactor),
                .dstColorBlendFactor = convert_to_vk(attachment.dstColorBlendFactor),
                .colorBlendOp = convert_to_vk(attachment.colorBlendOp),
                .srcAlphaBlendFactor = convert_to_vk(attachment.srcAlphaBlendFactor),
                .dstAlphaBlendFactor = convert_to_vk(attachment.dstAlphaBlendFactor),
                .alphaBlendOp = convert_to_vk(attachment.alphaBlendOp),
                .colorWriteMask = convert_to_vk(attachment.colorWriteMask),
            });
        }

        const VkPipelineColorBlendStateCreateInfo colorBlending{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = colorBlendAttachments.size32(),
            .pAttachments = colorBlendAttachments.data(),
            .blendConstants = {},
        };

        const VkPipelineDepthStencilStateCreateInfo depthStencil{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .flags = convert_to_vk(descriptor.depthStencilState.flags),
            .depthTestEnable = descriptor.depthStencilState.depthTestEnable,
            .depthWriteEnable = descriptor.depthStencilState.depthWriteEnable,
            .depthCompareOp = convert_to_vk(descriptor.depthStencilState.depthCompareOp),
            .depthBoundsTestEnable = descriptor.depthStencilState.depthBoundsTestEnable,
            .stencilTestEnable = descriptor.depthStencilState.stencilTestEnable,
            .front =
                {
                    .failOp = convert_to_vk(descriptor.depthStencilState.front.failOp),
                    .passOp = convert_to_vk(descriptor.depthStencilState.front.passOp),
                    .depthFailOp = convert_to_vk(descriptor.depthStencilState.front.depthFailOp),
                    .compareOp = convert_to_vk(descriptor.depthStencilState.front.compareOp),
                    .compareMask = descriptor.depthStencilState.front.compareMask,
                    .writeMask = descriptor.depthStencilState.front.writeMask,
                    .reference = descriptor.depthStencilState.front.reference,
                },
            .back =
                {
                    .failOp = convert_to_vk(descriptor.depthStencilState.back.failOp),
                    .passOp = convert_to_vk(descriptor.depthStencilState.back.passOp),
                    .depthFailOp = convert_to_vk(descriptor.depthStencilState.back.depthFailOp),
                    .compareOp = convert_to_vk(descriptor.depthStencilState.back.compareOp),
                    .compareMask = descriptor.depthStencilState.back.compareMask,
                    .writeMask = descriptor.depthStencilState.back.writeMask,
                    .reference = descriptor.depthStencilState.back.reference,
                },
            .minDepthBounds = descriptor.depthStencilState.minDepthBounds,
            .maxDepthBounds = descriptor.depthStencilState.maxDepthBounds,
        };

        constexpr VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        const VkPipelineDynamicStateCreateInfo dynamicState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = array_size32(dynamicStates),
            .pDynamicStates = dynamicStates,
        };

        const VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &pipelineRenderingCreateInfo,
            .stageCount = stageCreateInfo.size32(),
            .pStages = stageCreateInfo.data(),
            .pVertexInputState = &vertexBufferInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = newPipeline->pipelineLayout,
            .renderPass = nullptr,
            .subpass = 0,
            .basePipelineHandle = nullptr,
            .basePipelineIndex = -1,
        };

        const VkResult result = vkCreateGraphicsPipelines(m_device,
            nullptr,
            1,
            &pipelineInfo,
            m_allocator.get_allocation_callbacks(),
            &newPipeline->pipeline);

        if (result != VK_SUCCESS)
        {
            return translate_error(result);
        }

        label_vulkan_object(newPipeline->pipeline, descriptor.debugLabel);

        // Success
        cleanup.cancel();

        return handle;
    }

    void vulkan_instance::destroy(h32<graphics_pipeline> handle)
    {
        auto& pipeline = m_renderPipelines.at(handle);
        destroy_pipeline_base(pipeline, m_device, m_allocator.get_allocation_callbacks());
        m_renderPipelines.erase(handle);
    }

    result<h32<compute_pipeline>> vulkan_instance::create_compute_pipeline(
        const compute_pipeline_descriptor& descriptor)
    {
        const auto [newPipeline, handle] = m_computePipelines.emplace();
        newPipeline->label = descriptor.debugLabel;

        auto cleanup = finally_if_not_cancelled([this, handle] { destroy(handle); });

        const expected pipelineLayoutRes = create_pipeline_layout(descriptor.pushConstants,
            descriptor.bindGroupLayouts,
            &newPipeline->pipelineLayout,
            descriptor.debugLabel);

        if (!pipelineLayoutRes)
        {
            return pipelineLayoutRes.error();
        }

        const VkPipelineShaderStageCreateInfo shaderStageInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = unwrap_shader_module(descriptor.computeShader),
            .pName = "main",
        };

        const VkComputePipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = shaderStageInfo,
            .layout = newPipeline->pipelineLayout,
        };

        const VkResult result = vkCreateComputePipelines(m_device,
            nullptr,
            1,
            &pipelineInfo,
            m_allocator.get_allocation_callbacks(),
            &newPipeline->pipeline);

        if (result != VK_SUCCESS)
        {
            return translate_error(result);
        }

        label_vulkan_object(newPipeline->pipeline, descriptor.debugLabel);

        cleanup.cancel();

        return handle;
    }

    void vulkan_instance::destroy(h32<compute_pipeline> handle)
    {
        auto& pipeline = m_computePipelines.at(handle);
        destroy_pipeline_base(pipeline, m_device, m_allocator.get_allocation_callbacks());
        m_computePipelines.erase(handle);
    }

    result<h32<raytracing_pipeline>> vulkan_instance::create_raytracing_pipeline(
        const raytracing_pipeline_descriptor& descriptor)
    {
        const auto [newPipeline, handle] = m_raytracingPipelines.emplace();
        newPipeline->label = descriptor.debugLabel;

        auto cleanup = finally_if_not_cancelled([this, handle] { destroy(handle); });

        const expected pipelineLayoutRes = create_pipeline_layout(descriptor.pushConstants,
            descriptor.bindGroupLayouts,
            &newPipeline->pipelineLayout,
            descriptor.debugLabel);

        if (!pipelineLayoutRes)
        {
            return pipelineLayoutRes.error();
        }

        // Collect all shader stages
        buffered_array<VkPipelineShaderStageCreateInfo, 8> shaderStages;
        buffered_array<VkRayTracingShaderGroupCreateInfoKHR, 16> shaderGroups;

        const u32 groupsCount = u32{bool{descriptor.rayGenerationShader}} + u32(descriptor.missShaders.size()) +
            u32(descriptor.hitGroups.size());

        // Ray generation shader
        if (descriptor.rayGenerationShader)
        {
            const u32 shaderIdx = shaderStages.size32();

            shaderStages.push_back({
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                .module = unwrap_shader_module(descriptor.rayGenerationShader),
                .pName = "main",
            });

            shaderGroups.push_back({
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = shaderIdx,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
            });
        }

        // Miss shaders
        for (const h32<shader_module> missShader : descriptor.missShaders)
        {
            const u32 shaderIdx = shaderStages.size32();

            shaderStages.push_back({
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
                .module = unwrap_shader_module(missShader),
                .pName = "main",
            });

            shaderGroups.push_back({
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = shaderIdx,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
            });
        }

        // Hit groups
        for (const raytracing_hit_group_descriptor& hitGroup : descriptor.hitGroups)
        {
            u32 closestHitShaderIdx = VK_SHADER_UNUSED_KHR;
            u32 anyHitShaderIdx = VK_SHADER_UNUSED_KHR;
            u32 intersectionShaderIdx = VK_SHADER_UNUSED_KHR;

            for (const raytracing_hit_shader& shader : hitGroup.shaders)
            {
                const u32 shaderIdx = shaderStages.size32();

                switch (shader.stage)
                {
                case shader_stage::closest_hit:
                    if (closestHitShaderIdx != VK_SHADER_UNUSED_KHR)
                    {
                        return error::invalid_usage;
                    }

                    closestHitShaderIdx = shaderIdx;
                    break;

                case shader_stage::any_hit:
                    if (anyHitShaderIdx != VK_SHADER_UNUSED_KHR)
                    {
                        return error::invalid_usage;
                    }

                    anyHitShaderIdx = shaderIdx;
                    break;

                case shader_stage::intersection:
                    if (intersectionShaderIdx != VK_SHADER_UNUSED_KHR)
                    {
                        return error::invalid_usage;
                    }

                    intersectionShaderIdx = shaderIdx;
                    break;

                default:
                    return error::invalid_usage;
                }

                shaderStages.push_back({
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = convert_enum(shader.stage),
                    .module = unwrap_shader_module(shader.handle),
                    .pName = "main",
                });
            }

            const VkRayTracingShaderGroupTypeKHR groupType = convert_enum(hitGroup.type);

            shaderGroups.push_back({
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = groupType,
                .generalShader = VK_SHADER_UNUSED_KHR,
                .closestHitShader = closestHitShaderIdx,
                .anyHitShader = anyHitShaderIdx,
                .intersectionShader = intersectionShaderIdx,
            });
        }

        const VkRayTracingPipelineCreateInfoKHR pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
            .stageCount = shaderStages.size32(),
            .pStages = shaderStages.data(),
            .groupCount = shaderGroups.size32(),
            .pGroups = shaderGroups.data(),
            .maxPipelineRayRecursionDepth = descriptor.maxPipelineRayRecursionDepth,
            .layout = newPipeline->pipelineLayout,
        };

        const VkResult result = m_loadedFunctions.vkCreateRayTracingPipelinesKHR(m_device,
            nullptr,
            nullptr,
            1u,
            &pipelineInfo,
            m_allocator.get_allocation_callbacks(),
            &newPipeline->pipeline);

        if (result != VK_SUCCESS)
        {
            return translate_error(result);
        }

        label_vulkan_object(newPipeline->pipeline, descriptor.debugLabel);

        // Create the shader buffer table

        const u32 handleSize = m_raytracingProperties.shaderGroupHandleSize;
        const u32 handleSizeAligned = round_up_multiple(handleSize, m_raytracingProperties.shaderGroupHandleAlignment);

        newPipeline->rayGen = {
            .stride = round_up_multiple(handleSizeAligned, m_raytracingProperties.shaderGroupBaseAlignment),
        };

        // Ray-generation is a special case, size has to match the stride
        newPipeline->rayGen.size = newPipeline->rayGen.stride;

        const u32 missCount = u32(descriptor.missShaders.size());

        if (missCount > 0)
        {
            newPipeline->miss = {
                .stride = handleSizeAligned,
                .size =
                    round_up_multiple(missCount * handleSizeAligned, m_raytracingProperties.shaderGroupBaseAlignment),
            };
        }

        u32 hitCount = 0;

        for (auto& hg : descriptor.hitGroups)
        {
            hitCount += u32(hg.shaders.size());
        }

        newPipeline->hit = {
            .stride = handleSizeAligned,
            .size = round_up_multiple(hitCount * handleSizeAligned, m_raytracingProperties.shaderGroupBaseAlignment),
        };

        const VkDeviceSize sbtBufferSize =
            newPipeline->rayGen.size + newPipeline->miss.size + newPipeline->hit.size + newPipeline->callable.size;

        const VkResult bindingTableResult = m_allocator.create_buffer(
            {
                .size = sbtBufferSize,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                    VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                .debugLabel = "Shader Binding Table",
            },
            &newPipeline->shaderBindingTable);

        if (bindingTableResult != VK_SUCCESS)
        {
            return translate_error(bindingTableResult);
        }

        void* sbtPtr;
        const VkResult mapResult = m_allocator.map(newPipeline->shaderBindingTable.allocation, &sbtPtr);

        if (mapResult != VK_SUCCESS)
        {
            return translate_error(mapResult);
        }

        const u32 handleCount = groupsCount;
        const u32 handleDataSize = handleCount * handleSize;
        dynamic_array<u8> handles;
        handles.resize_default(handleDataSize);

        const VkResult getHandlesResult = m_loadedFunctions.vkGetRayTracingShaderGroupHandlesKHR(m_device,
            newPipeline->pipeline,
            0,
            handleCount,
            handleDataSize,
            handles.data());

        if (getHandlesResult != VK_SUCCESS)
        {
            return translate_error(getHandlesResult);
        }

        const VkBufferDeviceAddressInfo deviceAddressInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = newPipeline->shaderBindingTable.buffer,
        };

        const VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(m_device, &deviceAddressInfo);

        newPipeline->rayGen.deviceAddress = sbtAddress;
        newPipeline->miss.deviceAddress = newPipeline->rayGen.deviceAddress + newPipeline->rayGen.size;
        newPipeline->hit.deviceAddress = newPipeline->miss.deviceAddress + newPipeline->miss.size;
        newPipeline->callable.deviceAddress = newPipeline->hit.deviceAddress + newPipeline->hit.size;

        struct group_desc
        {
            VkStridedDeviceAddressRegionKHR* group;
            u32 groupHandles;
        };

        const group_desc groupsWithCount[] = {
            {&newPipeline->rayGen, 1},
            {&newPipeline->miss, missCount},
            {
                &newPipeline->hit,
                hitCount,
            },
            {&newPipeline->callable, 0},
        };

        u32 nextHandleIndex = 0;

        for (auto const [group, numHandles] : groupsWithCount)
        {
            if (group->size == 0)
            {
                continue;
            }

            const auto offset = group->deviceAddress - sbtAddress;

            for (u32 i = 0; i < numHandles; ++i)
            {
                const auto dstOffset = offset + i * handleSizeAligned;
                const auto srcOffset = nextHandleIndex * handleSize;

                std::memcpy(static_cast<u8*>(sbtPtr) + dstOffset, handles.data() + srcOffset, handleSize);

                ++nextHandleIndex;
            }
        }

        m_allocator.unmap(newPipeline->shaderBindingTable.allocation);
        m_allocator.invalidate_mapped_memory_ranges({&newPipeline->shaderBindingTable.allocation, 1});

        cleanup.cancel();

        return handle;
    }

    void vulkan_instance::destroy(h32<raytracing_pipeline> handle)
    {
        auto& pipeline = m_raytracingPipelines.at(handle);
        destroy_pipeline_base(pipeline, m_device, m_allocator.get_allocation_callbacks());
        m_raytracingPipelines.erase(handle);
    }

    namespace
    {
        VkRenderingAttachmentInfo make_rendering_attachment_info(
            VkImageView imageView, VkImageLayout layout, const graphics_attachment& attachment)
        {
            return {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = imageView,
                .imageLayout = layout,
                .loadOp = convert_to_vk(attachment.loadOp),
                .storeOp = convert_to_vk(attachment.storeOp),
                .clearValue = {std::bit_cast<VkClearColorValue>(attachment.clearValue)},
            };
        }
    }

    result<hptr<graphics_pass>> vulkan_instance::begin_graphics_pass(
        hptr<command_buffer> cmdBuffer, h32<graphics_pipeline> pipeline, const graphics_pass_descriptor& descriptor)
    {
        auto* const p = m_renderPipelines.try_find(pipeline);

        if (!p)
        {
            return error::invalid_handle;
        }

        buffered_array<VkRenderingAttachmentInfo, 2> colorAttachments;

        for (const auto& colorAttachment : descriptor.colorAttachments)
        {
            const VkImageView view = unwrap_image_view(colorAttachment.image);

            colorAttachments.push_back(
                make_rendering_attachment_info(view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorAttachment));
        }

        std::optional<VkRenderingAttachmentInfo> depthAttachment;

        if (descriptor.depthAttachment)
        {
            const VkImageView view = unwrap_image_view(descriptor.depthAttachment->image);

            depthAttachment = make_rendering_attachment_info(view,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                *descriptor.depthAttachment);
        }

        std::optional<VkRenderingAttachmentInfo> stencilAttachment;

        if (descriptor.stencilAttachment)
        {
            const VkImageView view = unwrap_image_view(descriptor.stencilAttachment->image);

            stencilAttachment = make_rendering_attachment_info(view,
                VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
                *descriptor.stencilAttachment);
        }

        const VkRenderingInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea =
                {
                    .offset = {descriptor.renderOffset.x, descriptor.renderOffset.y},
                    .extent = {descriptor.renderResolution.x, descriptor.renderResolution.y},
                },
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = colorAttachments.size32(),
            .pColorAttachments = colorAttachments.data(),
            .pDepthAttachment = depthAttachment ? &*depthAttachment : nullptr,
            .pStencilAttachment = stencilAttachment ? &*stencilAttachment : nullptr,
        };

        const VkCommandBuffer vkCommandBuffer = unwrap_handle<VkCommandBuffer>(cmdBuffer);

        vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipeline);
        vkCmdBeginRendering(vkCommandBuffer, &renderingInfo);

        // TODO: Bind descriptors
        // TODO: We might want context in some cases, e.g. with profiling active

        return hptr<graphics_pass>{pipeline.value};
    }

    void vulkan_instance::end_graphics_pass(hptr<command_buffer> cmdBuffer, hptr<graphics_pass>)
    {
        vkCmdEndRendering(unwrap_handle<VkCommandBuffer>(cmdBuffer));
    }

    result<hptr<compute_pass>> vulkan_instance::begin_compute_pass(hptr<command_buffer> cmdBuffer,
        h32<compute_pipeline> pipeline)
    {
        auto* const p = m_computePipelines.try_find(pipeline);

        if (!p)
        {
            return error::invalid_handle;
        }

        const VkCommandBuffer vkCommandBuffer = unwrap_handle<VkCommandBuffer>(cmdBuffer);
        vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipeline);

        // TODO: Bind descriptors

        m_cmdLabeler.begin(vkCommandBuffer, p->label.get());

        return hptr<compute_pass>{pipeline.value};
    }

    void vulkan_instance::end_compute_pass(hptr<command_buffer> cmdBuffer, hptr<compute_pass>)
    {
        m_cmdLabeler.end(unwrap_handle<VkCommandBuffer>(cmdBuffer));
    }

    result<hptr<raytracing_pass>> vulkan_instance::begin_raytracing_pass(hptr<command_buffer> cmdBuffer,
        h32<raytracing_pipeline> pipeline)
    {
        auto* const p = m_raytracingPipelines.try_find(pipeline);

        if (!p)
        {
            return error::invalid_handle;
        }

        const VkCommandBuffer vkCommandBuffer = unwrap_handle<VkCommandBuffer>(cmdBuffer);
        vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, p->pipeline);

        // TODO: Bind descriptors
        // TODO: Begin label

        m_cmdLabeler.begin(vkCommandBuffer, p->label.get());

        // This is used in trace_rays, if we change this we need to update that too
        return hptr<raytracing_pass>{pipeline.value};
    }

    void vulkan_instance::end_raytracing_pass(hptr<command_buffer> cmdBuffer, hptr<raytracing_pass>)
    {
        m_cmdLabeler.end(unwrap_handle<VkCommandBuffer>(cmdBuffer));
    }

    result<> vulkan_instance::begin_submit_tracking()
    {
        return begin_tracked_queue_submit();
    }

    result<> vulkan_instance::submit(h32<queue> queue, const queue_submit_descriptor& descriptor)
    {
        buffered_array<VkSemaphore, 8> waitSemaphores;
        buffered_array<VkSemaphore, 8> signalSemaphores;
        buffered_array<VkPipelineStageFlags, 8> waitStages;

        if (!descriptor.waitSemaphores.empty())
        {
            // TODO: This is not generic enough for all use cases
            waitStages.assign(descriptor.waitSemaphores.size(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

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

        OBLO_ASSERT(queue != universal_queue_id || !descriptor.signalFence,
            "For the universal queue, we need to override with internal fence for tracking");

        if (queue == universal_queue_id)
        {
            vkFence = m_fences.at(get_tracked_queue_fence());
        }
        else if (descriptor.signalFence)
        {
            vkFence = m_fences.at(descriptor.signalFence);
        }

        const auto result = translate_result(vkQueueSubmit(get_queue(queue).queue, 1, &submitInfo, vkFence));

        if (result)
        {
            const u64 submitIndex = get_submit_index();
            m_perFrameSetPool.on_submit(submitIndex);
            end_tracked_queue_submit();
        }

        return result;
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

    result<void*> vulkan_instance::memory_map(h32<buffer> buffer)
    {
        const VmaAllocation allocation = m_buffers.at(buffer).allocation;

        void* ptr{};
        return translate_error_or_value(m_allocator.map(allocation, &ptr), ptr);
    }

    result<> vulkan_instance::memory_unmap(h32<buffer> buffer)
    {
        const VmaAllocation allocation = m_buffers.at(buffer).allocation;
        m_allocator.unmap(allocation);
        return no_error;
    }

    result<> vulkan_instance::memory_invalidate(std::span<const h32<buffer>> buffers)
    {
        buffered_array<VmaAllocation, 8> allocations;
        allocations.reserve(buffers.size());

        for (const h32 b : buffers)
        {
            const VmaAllocation allocation = m_buffers.at(b).allocation;
            allocations.emplace_back(allocation);
        }

        return translate_result(m_allocator.invalidate_mapped_memory_ranges(allocations));
    }

    void vulkan_instance::cmd_copy_buffer(
        hptr<command_buffer> cmd, h32<buffer> src, h32<buffer> dst, std::span<const buffer_copy_descriptor> copies)
    {
        const VkCommandBuffer vkCmd = unwrap_handle<VkCommandBuffer>(cmd);

        buffered_array<VkBufferCopy, 8> regions;
        regions.reserve(copies.size());

        for (const auto& copy : copies)
        {
            regions.push_back({
                .srcOffset = copy.srcOffset,
                .dstOffset = copy.dstOffset,
                .size = copy.size,
            });
        }

        vkCmdCopyBuffer(vkCmd, m_buffers.at(src).buffer, m_buffers.at(dst).buffer, regions.size32(), regions.data());
    }

    void vulkan_instance::cmd_copy_buffer_to_image(
        hptr<command_buffer> cmd, h32<buffer> src, h32<image> dst, std::span<const buffer_image_copy_descriptor> copies)
    {
        auto& imageImpl = m_images.at(dst);

        const VkCommandBuffer vkCmd = unwrap_handle<VkCommandBuffer>(cmd);

        buffered_array<VkBufferImageCopy, 8> regions;
        regions.reserve(copies.size());

        const auto aspectMask = image_utils::deduce_aspect_mask(convert_enum(imageImpl.descriptor.format));

        for (const auto& copy : copies)
        {

            regions.push_back({
                .bufferOffset = copy.bufferOffset,
                .bufferRowLength = copy.bufferRowLength,
                .bufferImageHeight = copy.bufferImageHeight,
                .imageSubresource =
                    {
                        .aspectMask = aspectMask,
                        .mipLevel = copy.imageSubresource.mipLevel,
                        .baseArrayLayer = copy.imageSubresource.baseArrayLayer,
                        .layerCount = copy.imageSubresource.layerCount,
                    },
                .imageOffset = std::bit_cast<VkOffset3D>(copy.imageOffset),
                .imageExtent = std::bit_cast<VkExtent3D>(copy.imageExtent),
            });
        }

        vkCmdCopyBufferToImage(vkCmd,
            m_buffers.at(src).buffer,
            imageImpl.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            regions.size32(),
            regions.data());
    }

    void vulkan_instance::cmd_blit(hptr<command_buffer> cmd, h32<image> src, h32<image> dst, gpu::sampler_filter filter)
    {
        const auto& srcImpl = m_images.at(src);
        const auto& dstImpl = m_images.at(dst);

        VkImageBlit regions[1] = {
            {
                .srcSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .layerCount = 1,
                    },
                .srcOffsets =
                    {
                        {0, 0, 0},
                        {
                            i32(srcImpl.descriptor.width),
                            i32(srcImpl.descriptor.width),
                            i32(srcImpl.descriptor.depth),
                        },
                    },
                .dstSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .layerCount = 1,
                    },
                .dstOffsets =
                    {
                        {0, 0, 0},
                        {
                            i32(dstImpl.descriptor.width),
                            i32(dstImpl.descriptor.width),
                            i32(dstImpl.descriptor.depth),
                        },
                    },
            },
        };

        vkCmdBlitImage(unwrap_handle<VkCommandBuffer>(cmd),
            srcImpl.image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstImpl.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            regions,
            convert_enum(filter));
    }

    void vulkan_instance::cmd_apply_barriers(hptr<command_buffer> cmd, const memory_barrier_descriptor& descriptor)
    {
        buffered_array<VkImageMemoryBarrier2, 32> imageBarriers;
        imageBarriers.reserve(descriptor.images.size());

        for (const image_state_transition& transition : descriptor.images)
        {
            const auto& imageImpl = m_images.at(transition.image);

            VkImageMemoryBarrier2& barrier = imageBarriers.emplace_back();
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.image = imageImpl.image;
            barrier.subresourceRange = {
                .aspectMask = image_utils::deduce_aspect_mask(convert_enum(imageImpl.descriptor.format)),
                .baseMipLevel = 0,
                .levelCount = imageImpl.descriptor.mipLevels,
                .baseArrayLayer = 0,
                .layerCount = imageImpl.descriptor.arrayLayers,
            };
            deduce_barrier(barrier, transition);
        }

        const bool hasBufferBarriers = !descriptor.buffers.empty();

        buffered_array<VkMemoryBarrier2, 4> memoryBarriers;
        memoryBarriers.reserve(descriptor.memory.size() + u32{hasBufferBarriers});

        for (const global_memory_barrier& memoryBarrier : descriptor.memory)
        {
            VkMemoryBarrier2& barrier = memoryBarriers.emplace_back();
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            deduce_barrier(barrier, memoryBarrier);
        }

        // Very low effort, we just make a global memory barrier out of the buffer barriers for now
        // It does not matter much, especially as long as we use 1 queue

        if (hasBufferBarriers)
        {
            global_memory_barrier globalBarrier{};

            for (const buffer_memory_barrier& bufferBarrier : descriptor.buffers)
            {
                globalBarrier.previousPipelines |= bufferBarrier.previousPipelines;
                globalBarrier.previousAccesses |= bufferBarrier.previousAccesses;
                globalBarrier.nextPipelines |= bufferBarrier.nextPipelines;
                globalBarrier.nextAccesses |= bufferBarrier.nextAccesses;
            }

            VkMemoryBarrier2& barrier = memoryBarriers.emplace_back();
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            deduce_barrier(barrier, globalBarrier);
        }

        const VkDependencyInfo dependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = memoryBarriers.size32(),
            .pMemoryBarriers = memoryBarriers.data(),
            .imageMemoryBarrierCount = imageBarriers.size32(),
            .pImageMemoryBarriers = imageBarriers.data(),
        };

        vkCmdPipelineBarrier2(unwrap_handle<VkCommandBuffer>(cmd), &dependencyInfo);
    }

    void vulkan_instance::cmd_draw(
        hptr<command_buffer> cmd, u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance)
    {
        vkCmdDraw(unwrap_handle<VkCommandBuffer>(cmd), vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void vulkan_instance::cmd_draw_indexed(hptr<command_buffer> cmd,
        u32 indexCount,
        u32 instanceCount,
        u32 firstIndex,
        u32 vertexOffset,
        u32 firstInstance)
    {
        vkCmdDrawIndexed(unwrap_handle<VkCommandBuffer>(cmd),
            indexCount,
            instanceCount,
            firstIndex,
            vertexOffset,
            firstInstance);
    }

    void vulkan_instance::cmd_draw_mesh_tasks_indirect_count(hptr<command_buffer> cmd,
        h32<buffer> drawBuffer,
        u64 drawOffset,
        h32<buffer> countBuffer,
        u64 countOffset,
        u32 maxDrawCount)
    {
        const auto& drawBufferImpl = m_buffers.at(drawBuffer);
        const auto& countBufferImpl = m_buffers.at(countBuffer);

        m_loadedFunctions.vkCmdDrawMeshTasksIndirectCountEXT(unwrap_handle<VkCommandBuffer>(cmd),
            drawBufferImpl.buffer,
            drawOffset,
            countBufferImpl.buffer,
            countOffset,
            maxDrawCount,
            sizeof(VkDrawMeshTasksIndirectCommandEXT));
    }

    void vulkan_instance::cmd_dispatch_compute(hptr<command_buffer> cmd, u32 groupX, u32 groupY, u32 groupZ)
    {
        vkCmdDispatch(unwrap_handle<VkCommandBuffer>(cmd), groupX, groupY, groupZ);
    }

    void vulkan_instance::cmd_trace_rays(
        hptr<command_buffer> cmd, hptr<raytracing_pass> currentPass, u32 width, u32 height, u32 depth)
    {
        // Not ideal we could probably considering using the TLS or allocate this stuff into an arena
        const h32<raytracing_pipeline> h{u32(currentPass.value)};

        const auto& pipeline = m_raytracingPipelines.at(h);

        m_loadedFunctions.vkCmdTraceRaysKHR(unwrap_handle<VkCommandBuffer>(cmd),
            &pipeline.rayGen,
            &pipeline.miss,
            &pipeline.hit,
            &pipeline.callable,
            width,
            height,
            depth);
    }

    void vulkan_instance::cmd_set_viewport(
        hptr<command_buffer> cmd, u32 firstScissor, std::span<const rectangle> viewports, f32 minDepth, f32 maxDepth)
    {
        buffered_array<VkViewport, 8> vkViewports;
        vkViewports.reserve(viewports.size());

        for (const rectangle& r : viewports)
        {
            vkViewports.push_back({
                .x = f32(r.x),
                .y = f32(r.y),
                .width = f32(r.width),
                .height = f32(r.height),
                .minDepth = minDepth,
                .maxDepth = maxDepth,
            });
        }

        vkCmdSetViewport(unwrap_handle<VkCommandBuffer>(cmd), firstScissor, vkViewports.size32(), vkViewports.data());
    }

    void vulkan_instance::cmd_set_scissor(
        hptr<command_buffer> cmd, u32 firstScissor, std::span<const rectangle> scissors)
    {
        buffered_array<VkRect2D, 8> vkRects;
        vkRects.reserve(scissors.size());

        for (const rectangle& r : scissors)
        {
            vkRects.push_back({
                .offset = {r.x, r.y},
                .extent = {r.width, r.height},
            });
        }

        vkCmdSetScissor(unwrap_handle<VkCommandBuffer>(cmd), firstScissor, vkRects.size32(), vkRects.data());
    }

    void vulkan_instance::cmd_bind_index_buffer(
        hptr<command_buffer> cmd, h32<buffer> buffer, u64 offset, gpu::mesh_index_type format)
    {
        auto& bufferImpl = m_buffers.at(buffer);
        vkCmdBindIndexBuffer(unwrap_handle<VkCommandBuffer>(cmd), bufferImpl.buffer, offset, convert_enum(format));
    }

    namespace
    {
        void bind_descriptor_sets(const pipeline_base& base,
            VkCommandBuffer cmd,
            VkPipelineBindPoint bindPoint,
            u32 firstSet,
            std::span<const hptr<bind_group>> bindGroups)
        {
            vkCmdBindDescriptorSets(cmd,
                bindPoint,
                base.pipelineLayout,
                firstSet,
                u32(bindGroups.size()),
                reinterpret_cast<const VkDescriptorSet*>(bindGroups.data()),
                0,
                nullptr);
        }
    }

    void vulkan_instance::cmd_bind_groups(hptr<command_buffer> cmd,
        h32<graphics_pipeline> pipeline,
        u32 firstSet,
        std::span<const hptr<bind_group>> bindGroups)
    {
        auto& pipelineImpl = m_renderPipelines.at(pipeline);

        bind_descriptor_sets(pipelineImpl,
            unwrap_handle<VkCommandBuffer>(cmd),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            firstSet,
            bindGroups);
    }

    void vulkan_instance::cmd_bind_groups(hptr<command_buffer> cmd,
        h32<compute_pipeline> pipeline,
        u32 firstSet,
        std::span<const hptr<bind_group>> bindGroups)
    {
        auto& pipelineImpl = m_computePipelines.at(pipeline);

        bind_descriptor_sets(pipelineImpl,
            unwrap_handle<VkCommandBuffer>(cmd),
            VK_PIPELINE_BIND_POINT_COMPUTE,
            firstSet,
            bindGroups);
    }

    void vulkan_instance::cmd_bind_groups(hptr<command_buffer> cmd,
        h32<raytracing_pipeline> pipeline,
        u32 firstSet,
        std::span<const hptr<bind_group>> bindGroups)
    {
        auto& pipelineImpl = m_raytracingPipelines.at(pipeline);

        bind_descriptor_sets(pipelineImpl,
            unwrap_handle<VkCommandBuffer>(cmd),
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            firstSet,
            bindGroups);
    }

    void vulkan_instance::cmd_push_constants(hptr<command_buffer> cmd,
        h32<graphics_pipeline> pipeline,
        flags<shader_stage> shaderStages,
        u32 offset,
        std::span<const byte> data)
    {
        auto& pipelineImpl = m_renderPipelines.at(pipeline);

        vkCmdPushConstants(unwrap_handle<VkCommandBuffer>(cmd),
            pipelineImpl.pipelineLayout,
            convert_enum_flags(shaderStages),
            offset,
            u32(data.size()),
            data.data());
    }

    void vulkan_instance::cmd_push_constants(hptr<command_buffer> cmd,
        h32<compute_pipeline> pipeline,
        flags<shader_stage> shaderStages,
        u32 offset,
        std::span<const byte> data)
    {
        auto& pipelineImpl = m_computePipelines.at(pipeline);

        vkCmdPushConstants(unwrap_handle<VkCommandBuffer>(cmd),
            pipelineImpl.pipelineLayout,
            convert_enum_flags(shaderStages),
            offset,
            u32(data.size()),
            data.data());
    }

    void vulkan_instance::cmd_push_constants(hptr<command_buffer> cmd,
        h32<raytracing_pipeline> pipeline,
        flags<shader_stage> shaderStages,
        u32 offset,
        std::span<const byte> data)
    {
        auto& pipelineImpl = m_raytracingPipelines.at(pipeline);

        vkCmdPushConstants(unwrap_handle<VkCommandBuffer>(cmd),
            pipelineImpl.pipelineLayout,
            convert_enum_flags(shaderStages),
            offset,
            u32(data.size()),
            data.data());
    }

    void vulkan_instance::cmd_label_begin(hptr<command_buffer> cmd, const char* label)
    {
        m_cmdLabeler.begin(unwrap_handle<VkCommandBuffer>(cmd), label);
    }

    void vulkan_instance::cmd_label_end(hptr<command_buffer> cmd)
    {
        m_cmdLabeler.end(unwrap_handle<VkCommandBuffer>(cmd));
    }

    VkInstance vulkan_instance::get_instance() const
    {
        return m_instance;
    }

    VkPhysicalDevice vulkan_instance::get_physical_device() const
    {
        return m_physicalDevice;
    }

    VkDevice vulkan_instance::get_device() const
    {
        return m_device;
    }

    gpu_allocator& vulkan_instance::get_allocator()
    {
        return m_allocator;
    }

    VkAccelerationStructureKHR vulkan_instance::unwrap_acceleration_structure(h32<acceleration_structure> handle) const
    {
        return m_accelerationStructures.at(handle).vkAccelerationStructure;
    }

    VkBuffer vulkan_instance::unwrap_buffer(h32<buffer> handle) const
    {
        return m_buffers.at(handle).buffer;
    }

    VkCommandBuffer vulkan_instance::unwrap_command_buffer(hptr<command_buffer> handle) const
    {
        return unwrap_handle<VkCommandBuffer>(handle);
    }

    VkImage vulkan_instance::unwrap_image(h32<image> handle) const
    {
        return m_images.at(handle).image;
    }

    VkImageView vulkan_instance::unwrap_image_view(h32<image> handle) const
    {
        return m_images.at(handle).view;
    }

    VkQueue vulkan_instance::unwrap_queue(h32<queue> queue) const
    {
        return get_queue(queue).queue;
    }

    VkShaderModule vulkan_instance::unwrap_shader_module(h32<shader_module> handle) const
    {
        return m_shaderModules.at(handle).vkShaderModule;
    }

    debug_utils::object vulkan_instance::get_object_labeler() const
    {
        return m_objLabeler;
    }

    const loaded_functions& vulkan_instance::get_loaded_functions() const
    {
        return m_loadedFunctions;
    }

    h32<gpu::acceleration_structure> vulkan_instance::register_acceleration_structure(
        VkAccelerationStructureKHR accelerationStructure)
    {
        auto [it, handle] = m_accelerationStructures.emplace();
        it->vkAccelerationStructure = accelerationStructure;
        return handle;
    }

    h32<image> vulkan_instance::register_image(
        VkImage image, VkImageView view, VmaAllocation allocation, const image_descriptor& descriptor)
    {
        auto&& [img, handle] = m_images.emplace();

        *img = {
            {
                .image = image,
                .allocation = allocation,
            },
            view,
            descriptor,
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

    result<> vulkan_instance::create_pipeline_layout(std::span<const push_constant_range> pushConstants,
        std::span<const h32<bind_group_layout>> bindGroupLayouts,
        VkPipelineLayout* pipelineLayout,
        const debug_label& label)
    {
        buffered_array<VkDescriptorSetLayout, 4> descriptorSetLayouts;

        for (const h32 h : bindGroupLayouts)
        {
            const auto& bindGroup = m_bindGroupLayouts.at(h);
            descriptorSetLayouts.emplace_back(bindGroup.descriptorSetLayout);
        }

        buffered_array<VkPushConstantRange, 4> pushConstantRanges;

        for (const push_constant_range& pc : pushConstants)
        {
            pushConstantRanges.push_back({
                .stageFlags = convert_enum_flags(pc.stages),
                .offset = pc.offset,
                .size = pc.size,
            });
        }

        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = descriptorSetLayouts.size32(),
            .pSetLayouts = descriptorSetLayouts.data(),
            .pushConstantRangeCount = pushConstantRanges.size32(),
            .pPushConstantRanges = pushConstantRanges.data(),
        };

        const VkResult pipelineLayoutResult = vkCreatePipelineLayout(m_device,
            &pipelineLayoutInfo,
            m_allocator.get_allocation_callbacks(),
            pipelineLayout);

        if (pipelineLayoutResult != VK_SUCCESS)
        {
            return translate_error(pipelineLayoutResult);
        }

        label_vulkan_object(*pipelineLayout, label);
        return no_error;
    }

    void vulkan_instance::initialize_descriptor_set(VkDescriptorSet descriptorSet,
        std::span<const bind_group_data> data)
    {
        constexpr u32 MaxWrites{64};

        u32 buffersCount{0};
        u32 imagesCount{0};
        u32 writesCount{0};
        u32 accelerationStructuresCount{0};

        VkDescriptorBufferInfo bufferInfo[MaxWrites];
        VkDescriptorImageInfo imageInfo[MaxWrites];
        VkWriteDescriptorSet descriptorSetWrites[MaxWrites];
        VkWriteDescriptorSetAccelerationStructureKHR asSetWrites[MaxWrites];

        // We need a sampler even for bindless textures
        const VkSampler anySampler = get_or_create_dummy_sampler();

        for (const bind_group_data& bindingData : data)
        {
            switch (bindingData.object.kind)
            {
            case bindable_resource_kind::acceleration_structure: {
                const bindable_acceleration_structure& as = bindingData.object.accelerationStructure;

                OBLO_ASSERT(accelerationStructuresCount < MaxWrites);
                OBLO_ASSERT(writesCount < MaxWrites);

                const VkAccelerationStructureKHR accelerationStructure = unwrap_acceleration_structure(as);

                asSetWrites[accelerationStructuresCount] = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                    .accelerationStructureCount = 1,
                    .pAccelerationStructures = &accelerationStructure,
                };

                descriptorSetWrites[writesCount] = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = &asSetWrites[accelerationStructuresCount],
                    .dstSet = descriptorSet,
                    .dstBinding = bindingData.binding,
                    .descriptorCount = 1,
                    .descriptorType = convert_enum(bindingData.bindingKind),
                };

                ++accelerationStructuresCount;
                ++writesCount;
            }
            break;

            case bindable_resource_kind::buffer: {
                const bindable_buffer& buffer = bindingData.object.buffer;

                OBLO_ASSERT(buffersCount < MaxWrites);
                OBLO_ASSERT(writesCount < MaxWrites);
                OBLO_ASSERT(buffer.buffer);
                OBLO_ASSERT(buffer.size > 0);

                bufferInfo[buffersCount] = {
                    .buffer = unwrap_buffer(buffer.buffer),
                    .offset = buffer.offset,
                    .range = buffer.size,
                };

                descriptorSetWrites[writesCount] = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSet,
                    .dstBinding = bindingData.binding,
                    .descriptorCount = 1,
                    .descriptorType = convert_enum(bindingData.bindingKind),
                    .pBufferInfo = bufferInfo + buffersCount,
                };

                ++buffersCount;
                ++writesCount;
            }
            break;

            case bindable_resource_kind::image: {
                const bindable_image& image = bindingData.object.image;

                OBLO_ASSERT(imagesCount < MaxWrites);
                OBLO_ASSERT(writesCount < MaxWrites);

                const auto& imageImpl = m_images.at(image.image);

                imageInfo[imagesCount] = {
                    .sampler = anySampler,
                    .imageView = imageImpl.view,
                    .imageLayout = deduce_layout(image.state),
                };

                descriptorSetWrites[writesCount] = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSet,
                    .dstBinding = bindingData.binding,
                    .descriptorCount = 1,
                    .descriptorType = convert_enum(bindingData.bindingKind),
                    .pImageInfo = imageInfo + imagesCount,
                };

                ++imagesCount;
                ++writesCount;
            }
            break;
            }
        }

        if (writesCount > 0)
        {
            vkUpdateDescriptorSets(m_device, writesCount, descriptorSetWrites, 0, nullptr);
        }
    }

    VkSampler vulkan_instance::get_or_create_dummy_sampler()
    {
        if (!m_dummySampler)
        {
            const expected newSampler = create_sampler({});

            if (!newSampler)
            {
                return nullptr;
            }

            m_dummySampler = *newSampler;
        }

        return m_samplers.at(m_dummySampler).vkSampler;
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