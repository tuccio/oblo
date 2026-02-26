#include <oblo/vulkan/vulkan_engine_module.hpp>

#include <oblo/app/graphics_engine.hpp>
#include <oblo/app/graphics_window.hpp>
#include <oblo/app/graphics_window_context.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/vulkan/vulkan_instance.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/options/option_proxy.hpp>
#include <oblo/options/option_traits.hpp>
#include <oblo/options/options_module.hpp>
#include <oblo/options/options_provider.hpp>
#include <oblo/renderer/renderer.hpp>
#include <oblo/renderer/renderer_module.hpp>
#include <oblo/renderer/templates/graph_templates.hpp>
#include <oblo/trace/profile.hpp>

namespace oblo
{
    template <>
    struct option_traits<"r.requireHardwareRaytracing">
    {
        using type = bool;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::boolean,
            .id = "b01a7290-4f14-4b5c-9693-3b748bd9f45a"_uuid,
            .name = "Require Hardware Ray-Tracing",
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

        constexpr gpu::image_format g_SwapchainFormat = gpu::image_format::b8g8r8a8_unorm;

        bool create_swapchain_semaphores(
            gpu::gpu_instance& gpu, h32<gpu::semaphore> (&semaphores)[g_SwapchainImages], const char* debugName)
        {
            constexpr VkSemaphoreCreateInfo semaphoreInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            };

            string_builder nameBuilder;

            for (u32 i = 0; i < g_SwapchainImages; ++i)
            {
                const expected r = gpu.create_semaphore({.debugLabel = {debugName}});

                if (!r)
                {
                    return false;
                }

                semaphores[i] = r.value();
            }

            return true;
        }

        struct renderer_options
        {
            // We only read this at startup, any change requires a reset
            option_proxy<"r.requireHardwareRaytracing"> requireHardwareRaytracing;
        };

        struct vulkan_window_context final : public graphics_window_context
        {
            hptr<gpu::surface> surface{};
            h32<gpu::swapchain> swapchain{};

            h32<gpu::semaphore> acquiredImageSemaphores[g_SwapchainImages]{};

            u32 width{1};
            u32 height{1};

            h32<frame_graph_subgraph> swapchainGraph{};

            h32<frame_graph_subgraph> outGraph{};
            string outName;

            bool swapchainVisible = true;
            bool swapchainResized = false;
            bool markedForDestruction = false;

            bool initialize(gpu::gpu_instance& ctx, native_window_handle wh, u32 w, u32 h)
            {
                surface = ctx.create_surface(std::bit_cast<hptr<gpu::native_window>>(wh)).value_or({});

                if (!surface)
                {
                    shutdown(ctx);
                }

                width = w;
                height = h;

                if (!create_swapchain(ctx))
                {
                    shutdown(ctx);
                    return false;
                }

                if (!create_swapchain_semaphores(ctx,
                        acquiredImageSemaphores,
                        OBLO_STRINGIZE(vulkan_window_context::acquiredImageSemaphores)))
                {
                    return false;
                }

                return true;
            }

            bool create_swapchain(gpu::gpu_instance& ctx)
            {
                if (!ctx.create_swapchain({
                        .surface = surface,
                        .format = g_SwapchainFormat,
                        .width = width,
                        .height = height,
                    }))
                {
                    return false;
                }

                return true;
            }

            void destroy_swapchain(gpu::gpu_instance& ctx)
            {
                ctx.destroy(swapchain);
                swapchain = {};
            }

            void shutdown(gpu::gpu_instance& ctx)
            {
                // Should probably just delay defer destruction through the context instead
                ctx.wait_idle().assert_value();

                if (swapchain)
                {
                    destroy_swapchain(ctx);
                }

                if (surface)
                {
                    ctx.destroy(surface);
                    surface = {};
                }

                for (auto& semaphore : acquiredImageSemaphores)
                {
                    if (semaphore)
                    {
                        ctx.destroy(semaphore);
                        semaphore = {};
                    }
                }
            }

            expected<h32<gpu::image>> acquire_next_image(gpu::gpu_instance& ctx, u32 semaphoreIndex)
            {
                do
                {
                    const expected r = ctx.acquire_swapchain_image(swapchain, acquiredImageSemaphores[semaphoreIndex]);

                    if (r)
                    {
                        return *r;
                    }
                    else if (r.error() == gpu::error::out_of_date)
                    {
                        ctx.wait_idle().assert_value();
                        destroy_swapchain(ctx);

                        if (!create_swapchain(ctx))
                        {
                            return "Failed to create swapchain after it went out of date"_err;
                        }

                        // Try again
                        continue;
                    }
                    else
                    {
                        return "Failed to acquire image"_err;
                    }
                } while (true);
            }

            void on_visibility_change(bool visible) override
            {
                swapchainVisible = visible;
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

            void set_output(h32<frame_graph_subgraph> sg, string_view name) override
            {
                outGraph = sg;
                outName = name;
            }
        };
    }

    struct vulkan_engine_module::impl : public graphics_engine
    {
        gpu::vk::vulkan_instance ctx;

        renderer renderer;

        deque<unique_ptr<vulkan_window_context>> windowContexts;

        dynamic_array<h32<gpu::swapchain>> acquiredSwapchains;
        dynamic_array<h32<gpu::semaphore>> acquiredImageSemaphores;
        dynamic_array<vulkan_window_context*> contextsToRender;

        frame_graph_registry nodeRegistry;
        frame_graph_template swapchainGraphTemplate;

        u32 semaphoreIndex{};

        h32<gpu::semaphore> frameCompletedSemaphore[g_SwapchainImages]{};

        u64 presentDoneSubmitIndex[g_SwapchainImages]{};

        VkDebugUtilsMessengerEXT vkMessenger{};

        option_proxy_struct<renderer_options> options;
        bool isFullyInitialized{};

        bool initialize();
        void shutdown();

        void create_debug_callbacks();

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

    bool vulkan_engine_module::finalize()
    {
        return m_impl->initialize();
    }

    gpu::gpu_instance& vulkan_engine_module::get_gpu_instance()
    {
        return m_impl->ctx;
    }

    renderer& vulkan_engine_module::get_renderer()
    {
        return m_impl->renderer;
    }

    frame_graph& vulkan_engine_module::get_frame_graph()
    {
        return m_impl->renderer.get_frame_graph();
    }

    bool vulkan_engine_module::impl::initialize()
    {

        if (!ctx.init({
                .application = "oblo",
                .engine = "oblo",
            }))
        {
            return false;
        }

        create_debug_callbacks();

        // Then we need a surface to choose the queue and create the device
        graphics_window hiddenWindow;

        if (!hiddenWindow.create({.isHidden = true}))
        {
            return false;
        }

        const hptr hiddenWindowSurface =
            ctx.create_surface(std::bit_cast<hptr<gpu::native_window>>(hiddenWindow.get_native_handle())).value_or({});

        const auto cleanupSurface = finally([&] { ctx.destroy(hiddenWindowSurface); });

        auto& optionsManager = module_manager::get().find<options_module>()->manager();
        options.init(optionsManager);
        const bool requireHardwareRaytracing = options.requireHardwareRaytracing.read(optionsManager);

        const gpu::device_descriptor deviceDescriptor{
            .requireHardwareRaytracing = options.requireHardwareRaytracing.read(optionsManager),
        };

        if (!ctx.finalize_init(deviceDescriptor, hiddenWindowSurface))
        {
            return false;
        }

        isFullyInitialized = true;

        if (!create_swapchain_semaphores(ctx,
                frameCompletedSemaphore,
                OBLO_STRINGIZE(vulkan_engine::impl::frameCompletedSemaphore)))
        {
            return false;
        }

        if (!renderer.init({
                .gpu = ctx,
                .isRayTracingEnabled = requireHardwareRaytracing,
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
        if (isFullyInitialized)
        {
            ctx.wait_idle().assert_value();

            for (auto& windowContext : windowContexts)
            {
                windowContext->shutdown(ctx);
                windowContext.reset();
            }

            for (h32 semaphore : frameCompletedSemaphore)
            {
                ctx.destroy(semaphore);
            }

            renderer.shutdown();
        }

        isFullyInitialized = false;

        if (vkMessenger)
        {
            const PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
                reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(ctx.get_instance(), "vkDestroyDebugUtilsMessengerEXT"));

            vkDestroyDebugUtilsMessengerEXT(ctx.get_instance(), vkMessenger, nullptr);
            vkMessenger = {};
        }

        ctx.shutdown();
    }

    namespace
    {
        VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageTypes,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            [[maybe_unused]] void* pUserData)
        {
            log::severity severity = log::severity::debug;

            if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
            {
                severity = log::severity::error;
            }
            else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
            {
                severity = log::severity::warn;
            }
            else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0)
            {
                severity = log::severity::info;
            }

            log::generic(severity, "{}", pCallbackData->pMessage);

            return VK_FALSE;
        }
    }

    void vulkan_engine_module::impl::create_debug_callbacks()
    {
        // NOTE: The default allocator is used on purpose here, so we can log before creating it

        // VkDebugUtilsMessengerEXT
        {
            const PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
                reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(ctx.get_instance(), "vkCreateDebugUtilsMessengerEXT"));

            if (vkCreateDebugUtilsMessengerEXT)
            {
                const VkDebugUtilsMessengerCreateInfoEXT createInfo{
                    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                    .pfnUserCallback = debug_messenger_callback,
                };

                const auto result =
                    vkCreateDebugUtilsMessengerEXT(ctx.get_instance(), &createInfo, nullptr, &vkMessenger);

                if (result != VK_SUCCESS)
                {
                    log::error("Failed to create vkCreateDebugUtilsMessengerEXT (error: {0:#x})", i32{result});
                }
            }
            else
            {
                log::error("Unable to locate vkCreateDebugUtilsMessengerEXT");
            }
        }
    }

    graphics_window_context* vulkan_engine_module::impl::create_context(native_window_handle wh, u32 width, u32 height)
    {
        auto windowCtx = allocate_unique<vulkan_window_context>();

        if (!windowCtx->initialize(ctx, wh, width, height))
        {
            return nullptr;
        }

        return windowContexts.emplace_back(std::move(windowCtx)).get();
    }

    bool vulkan_engine_module::impl::acquire_images()
    {
        OBLO_PROFILE_SCOPE();

        auto& frameGraph = renderer.get_frame_graph();

        acquiredSwapchains.clear();
        acquiredImageSemaphores.clear();
        contextsToRender.clear();

        if (!ctx.wait_for_submit_completion(presentDoneSubmitIndex[semaphoreIndex]))
        {
            return false;
        }

        for (auto it = windowContexts.begin(); it != windowContexts.end();)
        {
            vulkan_window_context* const windowCtx = it->get();

            if (windowCtx->markedForDestruction)
            {
                // Collect it, it was destroyed
                windowCtx->shutdown(ctx);
                it = windowContexts.erase_unordered(it);
                continue;
            }

            if (!windowCtx->swapchainVisible)
            {
                ++it;
                continue;
            }

            if (windowCtx->swapchainResized)
            {
                windowCtx->destroy_swapchain(ctx);
                windowCtx->create_swapchain(ctx);
                windowCtx->swapchainResized = false;
                continue;
            }

            const expected image = windowCtx->acquire_next_image(ctx, semaphoreIndex);

            if (!image)
            {
                OBLO_ASSERT(false, "Failed to acquire image on swapchain");
                ++it;
                continue;
            }

            acquiredSwapchains.emplace_back(windowCtx->swapchain);
            acquiredImageSemaphores.emplace_back(windowCtx->acquiredImageSemaphores[semaphoreIndex]);

            auto swapChainGraph = frameGraph.instantiate(swapchainGraphTemplate);

            frameGraph.set_input(swapChainGraph, swapchain_graph::InAcquiredImage, *image).assert_value();

            contextsToRender.emplace_back(windowCtx);
            windowCtx->swapchainGraph = swapChainGraph;

            ++it;
        }

        if (acquiredSwapchains.empty())
        {
            return false;
        }

        renderer.begin_frame();

        return true;
    }

    void vulkan_engine_module::impl::present()
    {
        OBLO_PROFILE_SCOPE();

        auto& frameGraph = renderer.get_frame_graph();

        for (auto* context : contextsToRender)
        {
            frameGraph.set_output_state(context->swapchainGraph, swapchain_graph::OutPresentedImage, true);

            if (context->outGraph)
            {
                frameGraph.connect(context->outGraph,
                    context->outName,
                    context->swapchainGraph,
                    swapchain_graph::InRenderedImage);
            }
        }

        renderer.end_frame();

        const hptr commandBuffer = renderer.finalize_command_buffer_for_submission();

        presentDoneSubmitIndex[semaphoreIndex] = ctx.get_submit_index();

        acquiredImageSemaphores;

        ctx.submit(ctx.get_universal_queue(),
               {
                   .commandBuffers = {&commandBuffer, 1},
                   .waitSemaphores = acquiredImageSemaphores,
                   .signalSemaphores = {&frameCompletedSemaphore[semaphoreIndex], 1},
               })
            .assert_value();

        const expected presentResult = ctx.present({
            .swapchains = acquiredSwapchains,
            .waitSemaphores = {&frameCompletedSemaphore[semaphoreIndex], 1},
        });

        if (!presentResult && presentResult.error() != gpu::error::out_of_date)
        {
            log::error("Failed to present GPU back-buffer");
        }

        semaphoreIndex = (semaphoreIndex + 1) % g_SwapchainImages;

        for (auto* context : contextsToRender)
        {
            frameGraph.remove(context->swapchainGraph);
            context->swapchainGraph = {};
        }

        contextsToRender.clear();
    }
}