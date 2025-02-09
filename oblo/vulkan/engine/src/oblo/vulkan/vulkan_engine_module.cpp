#include <oblo/vulkan/vulkan_engine_module.hpp>

#include <oblo/app/graphics_engine.hpp>
#include <oblo/app/graphics_window.hpp>
#include <oblo/app/graphics_window_context.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/options/option_proxy.hpp>
#include <oblo/options/option_traits.hpp>
#include <oblo/options/options_module.hpp>
#include <oblo/options/options_provider.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/renderer_module.hpp>
#include <oblo/vulkan/swapchain.hpp>
#include <oblo/vulkan/templates/graph_templates.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo
{
    template <>
    struct option_traits<"r.isRayTracingEnabled">
    {
        using type = bool;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::boolean,
            .id = "b01a7290-4f14-4b5c-9693-3b748bd9f45a"_uuid,
            .name = "Enable Ray-Tracing",
            .category = "Graphics",
            .defaultValue = property_value_wrapper{true},
        };
    };
}

namespace oblo::vk
{
    namespace
    {
        constexpr u32 g_SwapchainImages = 3;
        constexpr VkFormat g_SwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;

        // TODO: Change finalize so that it may fail
        void return_error()
        {
            OBLO_ASSERT(false);
        }

        bool create_surface(native_window_handle wh,
            VkInstance instance,
            const VkAllocationCallbacks* allocator,
            VkSurfaceKHR* surface);

        bool create_swapchain_semaphores(VkDevice device,
            const gpu_allocator& allocator,
            VkSemaphore (&semaphores)[g_SwapchainImages],
            const char* debugName)
        {
            constexpr VkSemaphoreCreateInfo semaphoreInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            };

            string_builder nameBuilder;

            const auto debugUtilsObject = allocator.get_object_debug_utils();

            for (u32 i = 0; i < g_SwapchainImages; ++i)
            {
                if (vkCreateSemaphore(device, &semaphoreInfo, allocator.get_allocation_callbacks(), &semaphores[i]) !=
                    VK_SUCCESS)
                {
                    return false;
                }

                debugUtilsObject.set_object_name(device,
                    semaphores[i],
                    nameBuilder.clear().format("{}[{}]", debugName, i).c_str());
            }

            return true;
        }

        struct renderer_options
        {
            // We only read this at startup, any change requires a reset
            option_proxy<"r.isRayTracingEnabled"> isRayTracingEnabled;
        };

        struct vulkan_window_context final : public graphics_window_context
        {
            VkSurfaceKHR surface{};
            swapchain<g_SwapchainImages> swapchain;
            h32<texture> swapchainTextures[g_SwapchainImages]{};

            VkSemaphore acquiredImageSemaphores[g_SwapchainImages]{};

            u32 width{1};
            u32 height{1};

            h32<frame_graph_subgraph> swapchainGraph{};

            bool swapchainResized = false;
            bool markedForDestruction = false;

            bool initialize(
                vulkan_context& ctx, resource_manager& resourceManager, native_window_handle wh, u32 w, u32 h)
            {
                OBLO_ASSERT(!surface && !swapchain);

                if (!create_surface(wh, ctx.get_instance(), ctx.get_allocator().get_allocation_callbacks(), &surface))
                {
                    shutdown(ctx, resourceManager);
                    return false;
                }

                width = w;
                height = h;

                if (!create_swapchain(ctx, resourceManager))
                {
                    shutdown(ctx, resourceManager);
                    return false;
                }

                if (!create_swapchain_semaphores(ctx.get_device(),
                        ctx.get_allocator(),
                        acquiredImageSemaphores,
                        OBLO_STRINGIZE(vulkan_window_context::acquiredImageSemaphores)))
                {
                    return false;
                }

                return true;
            }

            bool create_swapchain(vulkan_context& ctx, resource_manager& resourceManager)
            {
                if (!swapchain.create(ctx, surface, width, height, g_SwapchainFormat))
                {
                    return false;
                }

                const image_initializer initializer{
                    .imageType = VK_IMAGE_TYPE_2D,
                    .format = g_SwapchainFormat,
                    .extent = VkExtent3D{.width = width, .height = height, .depth = 1},
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .tiling = VK_IMAGE_TILING_OPTIMAL,
                    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .memoryUsage = memory_usage::gpu_only,
                };

                for (u32 i = 0; i < g_SwapchainImages; ++i)
                {
                    swapchainTextures[i] = resourceManager.register_texture(
                        texture{
                            .image = swapchain.get_image(i),
                            .view = swapchain.get_image_view(i),
                            .initializer = initializer,
                        },
                        VK_IMAGE_LAYOUT_UNDEFINED);
                }

                return true;
            }

            void destroy_swapchain(vulkan_context& ctx, resource_manager& resourceManager)
            {
                swapchain.destroy(ctx);

                for (auto& handle : swapchainTextures)
                {
                    if (handle)
                    {
                        resourceManager.unregister_texture(handle);
                        handle = {};
                    }
                }
            }

            void shutdown(vulkan_context& ctx, resource_manager& resourceManager)
            {
                // Should probably just delay defer destruction through the context instead
                vkDeviceWaitIdle(ctx.get_device());

                if (swapchain)
                {
                    destroy_swapchain(ctx, resourceManager);
                }

                if (surface)
                {
                    vkDestroySurfaceKHR(ctx.get_instance(), surface, ctx.get_allocator().get_allocation_callbacks());

                    surface = nullptr;
                }

                for (auto semaphore : acquiredImageSemaphores)
                {
                    vkDestroySemaphore(ctx.get_device(), semaphore, ctx.get_allocator().get_allocation_callbacks());
                }
            }

            bool acquire_next_image(
                vulkan_context& ctx, resource_manager& resourceManager, u32 semaphoreIndex, u32* imageIndex)
            {
                VkResult acquireImageResult;

                do
                {
                    acquireImageResult = vkAcquireNextImageKHR(ctx.get_device(),
                        swapchain.get(),
                        UINT64_MAX,
                        acquiredImageSemaphores[semaphoreIndex],
                        VK_NULL_HANDLE,
                        imageIndex);

                    if (acquireImageResult == VK_SUCCESS)
                    {
                        break;
                    }
                    else if (acquireImageResult == VK_ERROR_OUT_OF_DATE_KHR)
                    {
                        vkDeviceWaitIdle(ctx.get_device());

                        destroy_swapchain(ctx, resourceManager);

                        if (!create_swapchain(ctx, resourceManager))
                        {
                            OBLO_ASSERT(false, "Failed to create swapchain after it went out of date");
                            return false;
                        }
                    }
                    else if (acquireImageResult != VK_SUCCESS)
                    {
                        OBLO_VK_PANIC_MSG("vkAcquireNextImageKHR", acquireImageResult);
                        return false;
                    }
                } while (true);

                return true;
            }

            // Implementation of graphics_window_context
            void on_resize(u32 w, u32 h) override
            {
                width = w;
                height = h;
                swapchainResized = true;
            }

            void on_destroy() override
            {
                markedForDestruction = true;
            }

            h32<frame_graph_subgraph> get_swapchain_graph() const override
            {
                return swapchainGraph;
            }
        };
    }

    struct vulkan_engine_module::impl : public graphics_engine
    {
        instance instance;
        single_queue_engine engine;
        gpu_allocator allocator;
        vulkan_context vkContext;

        // This is possibly not necessary anymore?
        resource_manager resourceManager;

        renderer renderer;

        deque<unique_ptr<vulkan_window_context>> windowContexts;

        dynamic_array<VkSwapchainKHR> acquiredSwapchains;
        dynamic_array<u32> acquiredImageIndices;
        dynamic_array<VkSemaphore> acquiredImageSemaphores;
        dynamic_array<h32<frame_graph_subgraph>> swapchainGraphs;

        frame_graph_registry nodeRegistry;
        frame_graph_template swapchainGraphTemplate;

        u32 semaphoreIndex{};

        VkSemaphore frameCompletedSemaphore[g_SwapchainImages]{};

        option_proxy_struct<renderer_options> options;

        bool initialize(const resource_registry& resourceRegistry);
        void shutdown();

        // Implementation of graphics_engine
        graphics_window_context* create_context(native_window_handle wh, u32 width, u32 height) override;

        bool acquire_images() override;
        void present() override;
    };

    vulkan_engine_module::vulkan_engine_module() = default;

    vulkan_engine_module::~vulkan_engine_module() = default;

    bool vulkan_engine_module::startup(const module_initializer& initializer)
    {
        m_impl = allocate_unique<impl>();

        initializer.services->add<impl>().as<graphics_engine>().externally_owned(m_impl.get());

        module_manager::get().load<renderer_module>();
        module_manager::get().load<options_module>();
        option_proxy_struct<renderer_options>::register_options(*initializer.services);

        return true;
    }

    void vulkan_engine_module::shutdown()
    {
        if (m_impl)
        {
            m_impl->shutdown();
            m_impl.reset();
        }
    }

    void vulkan_engine_module::finalize()
    {
        auto* const resourceRegistry = module_manager::get().find_unique_service<const resource_registry>();

        if (!resourceRegistry || !m_impl->initialize(*resourceRegistry))
        {
            return_error();
        }
    }

    vulkan_context& vulkan_engine_module::get_vulkan_context()
    {
        return m_impl->vkContext;
    }

    renderer& vulkan_engine_module::get_renderer()
    {
        return m_impl->renderer;
    }

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

        constexpr const char* g_rayTracingDeviceExtensions[] = {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        };
    }

    bool vulkan_engine_module::impl::initialize(const resource_registry& resourceRegistry)
    {
        // First we create the instance

        const VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "oblo",
            .applicationVersion = 0,
            .pEngineName = "oblo",
            .engineVersion = 0,
            .apiVersion = VK_API_VERSION_1_3,
        };

        if (!instance.init(appInfo, {}, g_instanceExtensions, nullptr))
        {
            return false;
        }

        // Then we need a surface to choose the queue and create the device
        graphics_window hiddenWindow;

        if (!hiddenWindow.create({.isHidden = true}))
        {
            return false;
        }

        VkSurfaceKHR surface{};

        if (!create_surface(hiddenWindow.get_native_handle(), instance.get(), nullptr, &surface))
        {
            return false;
        }

        const auto cleanup = finally([&] { vkDestroySurfaceKHR(instance.get(), surface, nullptr); });

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

        auto& optionsManager = module_manager::get().find<options_module>()->manager();
        options.init(optionsManager);
        const bool isRayTracingEnabled = options.isRayTracingEnabled.read(optionsManager);

        constexpr auto totalExtensions = array_size(requiredDeviceExtensions) + array_size(rayTracingExtensions);
        buffered_array<const char*, totalExtensions> deviceExtensions;
        deviceExtensions.append(std::begin(requiredDeviceExtensions), std::end(requiredDeviceExtensions));

        if (isRayTracingEnabled)
        {
            deviceExtensions.append(std::begin(rayTracingExtensions), std::end(rayTracingExtensions));
        }

        VkPhysicalDeviceFeatures2 g_physicalDeviceFeatures2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .features = g_physicalDeviceFeatures,
        };

        if (isRayTracingEnabled)
        {
            g_physicalDeviceFeatures2.pNext = &g_rtPipelineFeatures;
        }
        else
        {
            g_physicalDeviceFeatures2.pNext = &g_synchronizationFeatures;
        }

        if (!engine.init(instance.get(), surface, {}, deviceExtensions, &g_physicalDeviceFeatures2, nullptr))
        {
            return false;
        }

        if (!allocator.init(instance.get(), engine.get_physical_device(), engine.get_device()))
        {
            return false;
        }

        if (!vkContext.init({
                .instance = instance.get(),
                .engine = engine,
                .allocator = allocator,
                .resourceManager = resourceManager,
                .buffersPerFrame =
                    2, // This number includes the buffer for incomplete transition, so it's effectively half
                .submitsInFlight = g_SwapchainImages,
            }))
        {
            return false;
        }

        if (!create_swapchain_semaphores(engine.get_device(),
                allocator,
                frameCompletedSemaphore,
                OBLO_STRINGIZE(vulkan_engine::impl::frameCompletedSemaphore)))
        {
            return false;
        }

        if (!renderer.init({
                .vkContext = vkContext,
                .resources = resourceRegistry,
            }))
        {
            return false;
        }

        nodeRegistry = create_frame_graph_registry();
        swapchainGraphTemplate = swapchain_graph::create(nodeRegistry);

        return true;
    }

    void vulkan_engine_module::impl::shutdown()
    {
        if (VkDevice device = engine.get_device())
        {
            vkDeviceWaitIdle(device);

            renderer.shutdown();

            for (auto& windowContext : windowContexts)
            {
                windowContext->shutdown(vkContext, resourceManager);
                windowContext.reset();
            }

            for (VkSemaphore semaphore : frameCompletedSemaphore)
            {
                vkContext.reset_immediate(semaphore);
            }

            vkContext.shutdown();
            allocator.shutdown();
            engine.shutdown();
        }

        instance.shutdown();
    }

    graphics_window_context* vulkan_engine_module::impl::create_context(native_window_handle wh, u32 width, u32 height)
    {
        auto windowCtx = allocate_unique<vulkan_window_context>();

        if (!windowCtx->initialize(vkContext, resourceManager, wh, width, height))
        {
            return nullptr;
        }

        return windowContexts.emplace_back(std::move(windowCtx)).get();
    }

    bool vulkan_engine_module::impl::acquire_images()
    {
        auto& frameGraph = renderer.get_frame_graph();

        acquiredSwapchains.clear();
        acquiredImageIndices.clear();
        acquiredImageSemaphores.clear();
        swapchainGraphs.clear();

        vkContext.frame_begin(frameCompletedSemaphore[semaphoreIndex]);

        for (auto it = windowContexts.begin(); it != windowContexts.end();)
        {
            vulkan_window_context* const windowCtx = it->get();

            if (windowCtx->markedForDestruction)
            {
                // Collect it, it was destroyed
                windowCtx->shutdown(vkContext, resourceManager);
                it = windowContexts.erase_unordered(it);
                continue;
            }

            if (windowCtx->swapchainResized)
            {
                windowCtx->destroy_swapchain(vkContext, resourceManager);
                windowCtx->create_swapchain(vkContext, resourceManager);
                windowCtx->swapchainResized = false;
                continue;
            }

            u32 imageIndex;

            if (!windowCtx->acquire_next_image(vkContext, resourceManager, semaphoreIndex, &imageIndex))
            {
                OBLO_ASSERT(false, "Failed to acquire image on swapchain");
                ++it;
                continue;
            }

            acquiredSwapchains.emplace_back(windowCtx->swapchain.get());
            acquiredImageIndices.emplace_back(imageIndex);
            acquiredImageSemaphores.emplace_back(windowCtx->acquiredImageSemaphores[semaphoreIndex]);

            auto swapChainGraph = frameGraph.instantiate(swapchainGraphTemplate);

            frameGraph
                .set_input(swapChainGraph, swapchain_graph::InAcquiredImage, windowCtx->swapchainTextures[imageIndex])
                .assert_value();

            swapchainGraphs.emplace_back(swapChainGraph);
            windowCtx->swapchainGraph = swapChainGraph;

            ++it;
        }

        if (acquiredSwapchains.empty())
        {
            vkContext.frame_end();
            return false;
        }

        vkContext.push_frame_wait_semaphores(acquiredImageSemaphores);

        renderer.begin_frame();

        return true;
    }

    void vulkan_engine_module::impl::present()
    {
        auto& frameGraph = renderer.get_frame_graph();

        for (auto& swapchainGraph : swapchainGraphs)
        {
            frameGraph.set_output_state(swapchainGraph, swapchain_graph::OutPresentedImage, true);
        }

        renderer.end_frame();

        // Should be unnecessary, but we won't submit if we don't call it at least once
        vkContext.get_active_command_buffer();

        vkContext.frame_end();

        const VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &frameCompletedSemaphore[semaphoreIndex],
            .swapchainCount = acquiredSwapchains.size32(),
            .pSwapchains = acquiredSwapchains.data(),
            .pImageIndices = acquiredImageIndices.data(),
            .pResults = nullptr,
        };

        OBLO_VK_PANIC_EXCEPT(vkQueuePresentKHR(engine.get_queue(), &presentInfo), VK_ERROR_OUT_OF_DATE_KHR);

        semaphoreIndex = (semaphoreIndex + 1) % g_SwapchainImages;

        for (auto& swapchainGraph : swapchainGraphs)
        {
            // TODO: Just clear the input edges instead of removing it every time
            frameGraph.remove(swapchainGraph);
        }

        swapchainGraphs.clear();
    }
}

#ifdef WIN32
    #include <Windows.h>

    #include <vulkan/vulkan_win32.h>

namespace oblo::vk
{
    namespace
    {
        bool create_surface(
            native_window_handle wh, VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface)
        {
            const HWND hwnd = reinterpret_cast<HWND>(wh);

            const VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{
                .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                .hinstance = GetModuleHandle(nullptr),
                .hwnd = hwnd,
            };

            if (vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, allocator, surface) != VK_SUCCESS)
            {
                return false;
            }

            return true;
        }
    }
}
#endif