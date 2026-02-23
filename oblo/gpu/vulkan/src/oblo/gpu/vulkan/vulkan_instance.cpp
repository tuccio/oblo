#include <oblo/gpu/vulkan/vulkan_instance.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/overload.hpp>
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
        dynamic_array<descriptor_binding> bindings;
    };

    // Pipelines

    namespace
    {
        struct pipeline_base
        {
            VkPipeline pipeline;
            VkPipelineLayout pipelineLayout;
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

    struct vulkan_instance::render_pipeline_impl final : pipeline_base
    {
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
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

        // Cache the properties so users can query them on demand
        m_subgroupProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,
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

        label_vulkan_object(vkFence, descriptor.debugLabel);

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

        label_vulkan_object(semaphore, descriptor.debugLabel);

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

    result<h32<bind_group_layout>> vulkan_instance::create_bind_group_layout(
        const bind_group_layout_descriptor& descriptor)
    {
        auto [it, handle] = m_bindGroupLayouts.emplace();

        it->bindings.reserve(descriptor.bindings.size());

        for (const auto& binding : descriptor.bindings)
        {
            it->bindings.push_back({
                .binding = binding.binding,
                .descriptorType = convert_enum(binding.bindingKind),
                .stageFlags = convert_enum_flags(binding.shaderStages),
                .readOnly = binding.readOnly,
            });
        }

        const expected r = m_descriptorSetPool.get_or_add_layout(it->bindings);

        if (!r)
        {
            m_bindGroupLayouts.erase(handle);
            return r.error();
        }

        it->descriptorSetLayout = *r;
        return handle;
    }

    void vulkan_instance::destroy_bind_group_layout(h32<bind_group_layout> handle)
    {
        // TODO: Right now we never destroy the descriptor set layouts, probably sharing them is not a good idea, or
        // they would at least need to be ref-counted
        m_bindGroupLayouts.erase(handle);
    }

    result<hptr<bind_group>> vulkan_instance::acquire_transient_bind_group(h32<bind_group_layout> handle)
    {
        const VkDescriptorSetLayout layout = m_bindGroupLayouts.at(handle).descriptorSetLayout;
        const result r = m_descriptorSetPool.acquire(get_last_finished_submit(), layout, nullptr);

        if (!r)
        {
            return r.error();
        }

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

    void vulkan_instance::destroy_buffer(h32<buffer> bufferHandle)
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
            destroy_image(h);
            return view.error();
        }

        it->view = *view;

        label_vulkan_object(*view, descriptor.debugLabel);

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

    void vulkan_instance::destroy_image_pool(h32<image_pool> imagePoolHandle)
    {
        const image_pool_impl& pool = m_imagePools.at(imagePoolHandle);

        for (const h32 image : pool.images)
        {
            destroy_image(image);
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

    void vulkan_instance::destroy_shader_module(h32<shader_module> handle)
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
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = descriptor.minLod,
            .maxLod = descriptor.maxLod,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
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

    void vulkan_instance::destroy_sampler(h32<sampler> handle)
    {
        sampler_impl& impl = m_samplers.at(handle);
        vkDestroySampler(m_device, impl.vkSampler, m_allocator.get_allocation_callbacks());
        m_samplers.erase(handle);
    }

    result<h32<render_pipeline>> vulkan_instance::create_render_pipeline(const render_pipeline_descriptor& desc)
    {
        const auto [newPipeline, handle] = m_renderPipelines.emplace();

        auto cleanup = finally_if_not_cancelled([this, handle] { destroy_render_pipeline(handle); });

        buffered_array<VkDescriptorSetLayout, 4> descriptorSetLayouts;

        for (const h32 h : desc.bindGroupLayouts)
        {
            const auto& bindGroup = m_bindGroupLayouts.at(h);
            descriptorSetLayouts.emplace_back(bindGroup.descriptorSetLayout);
        }

        buffered_array<VkPushConstantRange, 4> pushConstantRanges;

        for (const push_constant_range& pc : desc.pushConstants)
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
            &newPipeline->pipelineLayout);

        if (pipelineLayoutResult != VK_SUCCESS)
        {
            return translate_error(pipelineLayoutResult);
        }

        label_vulkan_object(newPipeline->pipelineLayout, desc.debugLabel);

        buffered_array<VkPipelineShaderStageCreateInfo, 4> stageCreateInfo;

        for (const auto& stage : desc.stages)
        {
            stageCreateInfo.push_back({
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = convert_enum(stage.stage),
                .module = unwrap_shader_module(stage.shaderModule),
                .pName = stage.entryFunction,
            });
        }

        static_assert(sizeof(VkFormat) == sizeof(image_format), "We rely on this when reinterpret casting");

        const u32 numAttachments = u32(desc.renderTargets.colorAttachmentFormats.size());

        const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = numAttachments,
            .pColorAttachmentFormats =
                reinterpret_cast<const VkFormat*>(desc.renderTargets.colorAttachmentFormats.data()),
            .depthAttachmentFormat = convert_to_vk(desc.renderTargets.depthFormat),
            .stencilAttachmentFormat = convert_to_vk(desc.renderTargets.stencilFormat),
        };

        const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = convert_to_vk(desc.primitiveTopology),
            .primitiveRestartEnable = VK_FALSE,
        };

        buffered_array<VkVertexInputBindingDescription, 4> vertexInputBindingDescs;
        vertexInputBindingDescs.reserve(desc.vertexInputBindings.size());

        for (auto& binding : desc.vertexInputBindings)
        {
            vertexInputBindingDescs.push_back({
                .binding = binding.binding,
                .stride = binding.stride,
                .inputRate = convert_enum(binding.inputRate),
            });
        }

        buffered_array<VkVertexInputAttributeDescription, 4> vertexInputAttributes;
        vertexInputBindingDescs.reserve(desc.vertexInputAttributes.size());

        for (const auto& attribute : desc.vertexInputAttributes)
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
            .flags = convert_to_vk(desc.rasterizationState.flags),
            .depthClampEnable = desc.rasterizationState.depthClampEnable,
            .rasterizerDiscardEnable = desc.rasterizationState.rasterizerDiscardEnable,
            .polygonMode = convert_to_vk(desc.rasterizationState.polygonMode),
            .cullMode = convert_to_vk(desc.rasterizationState.cullMode),
            .frontFace = convert_to_vk(desc.rasterizationState.frontFace),
            .depthBiasEnable = desc.rasterizationState.depthBiasEnable,
            .depthBiasConstantFactor = desc.rasterizationState.depthBiasConstantFactor,
            .depthBiasClamp = desc.rasterizationState.depthBiasClamp,
            .depthBiasSlopeFactor = desc.rasterizationState.depthBiasSlopeFactor,
            .lineWidth = desc.rasterizationState.lineWidth,

        };

        const VkPipelineMultisampleStateCreateInfo multisampling{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .minSampleShading = 1.f,
        };

        buffered_array<VkPipelineColorBlendAttachmentState, 8> colorBlendAttachments;
        colorBlendAttachments.reserve(desc.renderTargets.blendStates.size());

        for (auto& attachment : desc.renderTargets.blendStates)
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
            .flags = convert_to_vk(desc.depthStencilState.flags),
            .depthTestEnable = desc.depthStencilState.depthTestEnable,
            .depthWriteEnable = desc.depthStencilState.depthWriteEnable,
            .depthCompareOp = convert_to_vk(desc.depthStencilState.depthCompareOp),
            .depthBoundsTestEnable = desc.depthStencilState.depthBoundsTestEnable,
            .stencilTestEnable = desc.depthStencilState.stencilTestEnable,
            .front =
                {
                    .failOp = convert_to_vk(desc.depthStencilState.front.failOp),
                    .passOp = convert_to_vk(desc.depthStencilState.front.passOp),
                    .depthFailOp = convert_to_vk(desc.depthStencilState.front.depthFailOp),
                    .compareOp = convert_to_vk(desc.depthStencilState.front.compareOp),
                    .compareMask = desc.depthStencilState.front.compareMask,
                    .writeMask = desc.depthStencilState.front.writeMask,
                    .reference = desc.depthStencilState.front.reference,
                },
            .back =
                {
                    .failOp = convert_to_vk(desc.depthStencilState.back.failOp),
                    .passOp = convert_to_vk(desc.depthStencilState.back.passOp),
                    .depthFailOp = convert_to_vk(desc.depthStencilState.back.depthFailOp),
                    .compareOp = convert_to_vk(desc.depthStencilState.back.compareOp),
                    .compareMask = desc.depthStencilState.back.compareMask,
                    .writeMask = desc.depthStencilState.back.writeMask,
                    .reference = desc.depthStencilState.back.reference,
                },
            .minDepthBounds = desc.depthStencilState.minDepthBounds,
            .maxDepthBounds = desc.depthStencilState.maxDepthBounds,
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

        label_vulkan_object(newPipeline->pipeline, desc.debugLabel);

        // Success
        cleanup.cancel();

        return handle;
    }

    void vulkan_instance::destroy_render_pipeline(h32<render_pipeline> handle)
    {
        auto& pipeline = m_renderPipelines.at(handle);
        destroy_pipeline_base(pipeline, m_device, m_allocator.get_allocation_callbacks());
        m_renderPipelines.erase(handle);
    }

    namespace
    {
        VkRenderingAttachmentInfo make_rendering_attachment_info(
            VkImageView imageView, VkImageLayout layout, const render_attachment& attachment)
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

    result<hptr<render_pass>> vulkan_instance::begin_render_pass(
        hptr<command_buffer> cmdBuffer, h32<render_pipeline> pipeline, const render_pass_descriptor& descriptor)
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
        // TODO: Returning an empty handle for now, we might want context in some cases, e.g. with profiling active

        return hptr<render_pass>{};
    }

    void vulkan_instance::end_render_pass(hptr<command_buffer> cmdBuffer, hptr<render_pass>)
    {
        vkCmdEndRendering(unwrap_handle<VkCommandBuffer>(cmdBuffer));
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
            m_descriptorSetPool.on_submit(get_submit_index());
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
        const VkCommandBuffer vkCmd = unwrap_handle<VkCommandBuffer>(cmd);

        buffered_array<VkBufferImageCopy, 8> regions;
        regions.reserve(copies.size());

        for (const auto& copy : copies)
        {
            regions.push_back({
                .bufferOffset = copy.bufferOffset,
                .bufferRowLength = copy.bufferRowLength,
                .bufferImageHeight = copy.bufferImageHeight,
                .imageSubresource =
                    {
                        .aspectMask = convert_enum_flags(copy.imageSubresource.aspectMask),
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
            m_images.at(dst).image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            regions.size32(),
            regions.data());
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

        buffered_array<VkMemoryBarrier2, 4> memoryBarriers;
        memoryBarriers.reserve(descriptor.memory.size());

        for (const global_memory_barrier& memoryBarrier : descriptor.memory)
        {
            VkMemoryBarrier2& barrier = memoryBarriers.emplace_back();
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            deduce_barrier(barrier, memoryBarrier);
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

    void vulkan_instance::cmd_label_begin(hptr<command_buffer> cmd, const char* label)
    {
        m_cmdLabeler.begin(unwrap_handle<VkCommandBuffer>(cmd), label);
    }

    void vulkan_instance::cmd_label_end(hptr<command_buffer> cmd)
    {
        m_cmdLabeler.end(unwrap_handle<VkCommandBuffer>(cmd));
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