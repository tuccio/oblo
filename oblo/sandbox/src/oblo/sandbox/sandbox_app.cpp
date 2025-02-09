#include <oblo/sandbox/sandbox_app.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/log/log.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/error.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <fstream>
#include <span>

namespace oblo::vk
{
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

    void sandbox_base::shutdown()
    {
        if (m_engine.get_device())
        {
            m_imgui.shutdown(m_engine.get_device());

            for (auto acquiredImage : m_acquiredImages)
            {
                m_context.reset_immediate(acquiredImage);
            }

            for (auto frameCompleted : m_frameCompleted)
            {
                m_context.reset_immediate(frameCompleted);
            }

            m_swapchain.destroy(m_context);
        }

        if (m_surface)
        {
            vkDestroySurfaceKHR(m_instance.get(), m_surface, nullptr);
        }

        m_context.shutdown();

        m_allocator.shutdown();
        m_engine.shutdown();

        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        destroy_debug_callbacks();
    }

    void sandbox_base::set_config(const sandbox_app_config& config)
    {
        m_config = config;
    }

    bool sandbox_base::init(std::span<const char* const> instanceExtensions,
        std::span<const char* const> instanceLayers,
        std::span<const char* const> deviceExtensions,
        void* deviceFeaturesList,
        const VkPhysicalDeviceFeatures2* physicalDeviceFeatures)
    {
        if (!create_window() ||
            !create_engine(instanceExtensions,
                instanceLayers,
                deviceExtensions,
                deviceFeaturesList,
                physicalDeviceFeatures))
        {
            return false;
        }

        int width, height;
        SDL_Vulkan_GetDrawableSize(m_window, &width, &height);

        m_renderWidth = u32(width);
        m_renderHeight = u32(height);

        if (!create_synchronization_objects())
        {
            return false;
        }

        if (!create_swapchain())
        {
            return false;
        }

        return init_imgui();
    }

    void sandbox_base::wait_idle()
    {
        auto* const device = m_engine.get_device();

        if (device)
        {
            vkDeviceWaitIdle(device);
        }
    }

    namespace
    {
        mouse_key sdl_map_mouse_key(u8 key)
        {
            switch (key)
            {
            case SDL_BUTTON_LEFT:
                return mouse_key::left;

            case SDL_BUTTON_RIGHT:
                return mouse_key::right;

            case SDL_BUTTON_MIDDLE:
                return mouse_key::middle;

            default:
                OBLO_ASSERT(false, "Unhandled mouse key");
                return mouse_key::enum_max;
            }
        }

        keyboard_key sdl_map_keyboard_key(SDL_Keycode key)
        {
            if (key >= 'a' && key <= 'z')
            {
                return keyboard_key(u32(keyboard_key::a) + (key - 'a'));
            }

            switch (key)
            {
            case SDLK_LSHIFT:
                return keyboard_key::left_shift;
            }

            return keyboard_key::enum_max;
        }

        time sdl_convert_time(u32 time)
        {
            return time::from_milliseconds(time);
        }
    }

    bool sandbox_base::poll_events()
    {
        OBLO_PROFILE_SCOPE();

        for (SDL_Event event; SDL_PollEvent(&event);)
        {
            if (m_showImgui)
            {
                m_imgui.process(event);
            }

            switch (event.type)
            {
            case SDL_QUIT:
                return false;

            case SDL_WINDOWEVENT:
                if (!handle_window_events(event))
                {
                    return false;
                }

                return true;
            }

            if (m_processInput)
            {
                switch (event.type)
                {
                case SDL_MOUSEBUTTONDOWN:
                    m_inputQueue.push({
                        .kind = input_event_kind::mouse_press,
                        .timestamp = sdl_convert_time(event.button.timestamp),
                        .mousePress =
                            {
                                .key = sdl_map_mouse_key(event.button.button),
                            },
                    });
                    break;

                case SDL_MOUSEBUTTONUP:
                    m_inputQueue.push({
                        .kind = input_event_kind::mouse_release,
                        .timestamp = sdl_convert_time(event.button.timestamp),
                        .mouseRelease =
                            {
                                .key = sdl_map_mouse_key(event.button.button),
                            },
                    });
                    break;

                case SDL_MOUSEMOTION:
                    m_inputQueue.push({
                        .kind = input_event_kind::mouse_move,
                        .timestamp = sdl_convert_time(event.motion.timestamp),
                        .mouseMove =
                            {
                                .x = f32(event.motion.x),
                                .y = f32(event.motion.y),
                            },
                    });
                    break;

                case SDL_KEYDOWN:
                    m_inputQueue.push({
                        .kind = input_event_kind::keyboard_press,
                        .timestamp = sdl_convert_time(event.key.timestamp),
                        .keyboardPress =
                            {
                                .key = sdl_map_keyboard_key(event.key.keysym.sym),
                            },
                    });
                    break;

                case SDL_KEYUP:
                    m_inputQueue.push({
                        .kind = input_event_kind::keyboard_release,
                        .timestamp = sdl_convert_time(event.key.timestamp),
                        .keyboardRelease =
                            {
                                .key = sdl_map_keyboard_key(event.key.keysym.sym),
                            },
                    });

                    break;
                }
            }
        }

        return true;
    }

    void sandbox_base::begin_frame(u32* outImageIndex)
    {
        m_context.frame_begin(m_acquiredImages[m_semaphoreIndex], m_frameCompleted[m_semaphoreIndex]);

        u32 imageIndex;
        VkResult acquireImageResult;

        do
        {
            OBLO_PROFILE_SCOPE("Acquire swapchain image");

            acquireImageResult = vkAcquireNextImageKHR(m_engine.get_device(),
                m_swapchain.get(),
                UINT64_MAX,
                m_acquiredImages[m_semaphoreIndex],
                VK_NULL_HANDLE,
                &imageIndex);

            if (acquireImageResult == VK_SUCCESS)
            {
                break;
            }
            else if (acquireImageResult == VK_ERROR_OUT_OF_DATE_KHR)
            {
                wait_idle();

                destroy_swapchain();

                int width, height;
                SDL_GetWindowSize(m_window, &width, &height);

                m_renderWidth = u32(width);
                m_renderHeight = u32(height);

                if (!create_swapchain())
                {
                    std::abort();
                }
            }
            else if (acquireImageResult != VK_SUCCESS)
            {
                OBLO_VK_PANIC_MSG("vkAcquireNextImageKHR", acquireImageResult);
                std::abort();
            }
        } while (true);

        *outImageIndex = imageIndex;
    }

    void sandbox_base::submit_and_present(u32 imageIndex)
    {
        OBLO_PROFILE_SCOPE();

        m_context.frame_end();

        const auto swapchain = m_swapchain.get();

        const VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_frameCompleted[m_semaphoreIndex],
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex,
            .pResults = nullptr,
        };

        OBLO_VK_PANIC_EXCEPT(vkQueuePresentKHR(m_engine.get_queue(), &presentInfo), VK_ERROR_OUT_OF_DATE_KHR);

        m_semaphoreIndex = (m_semaphoreIndex + 1) % SwapchainImages;
    }

    bool sandbox_base::run_frame_impl(void* instance, update_fn update, update_imgui_fn updateImgui)
    {
        OBLO_PROFILE_FRAME_BEGIN();

        if (!poll_events())
        {
            return false;
        }

        if (m_minimized)
        {
            return true;
        }

        u32 imageIndex;
        begin_frame(&imageIndex);

        const h32<texture> swapchainTexture = m_swapchainTextures[imageIndex];

        const sandbox_render_context context{
            .vkContext = &m_context,
            .swapchainTexture = swapchainTexture,
            .width = m_renderWidth,
            .height = m_renderHeight,
        };

        update(instance, context);

        auto& cb = m_context.get_active_command_buffer();

        if (m_showImgui)
        {
            OBLO_PROFILE_SCOPE("ImGui update");

            m_imgui.begin_frame();

            const sandbox_update_imgui_context imguiContext{
                .vkContext = &m_context,
            };

            updateImgui(instance, imguiContext);

            cb.add_pipeline_barrier(m_resourceManager, swapchainTexture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            m_imgui.end_frame(cb.get(), m_swapchain.get_image_view(imageIndex), m_renderWidth, m_renderHeight);
        }

        cb.add_pipeline_barrier(m_resourceManager, swapchainTexture, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        submit_and_present(imageIndex);

        OBLO_PROFILE_FRAME_END();

        return true;
    }

    bool sandbox_base::create_window()
    {
        auto windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;

        if (m_config.uiWindowMaximized)
        {
            windowFlags |= SDL_WINDOW_MAXIMIZED;
        }

        const i32 w = m_config.uiWindowWidth < 0 ? 1280u : m_config.uiWindowWidth;
        const i32 h = m_config.uiWindowHeight < 0 ? 720u : m_config.uiWindowHeight;

        m_window = SDL_CreateWindow(m_config.appMainWindowTitle,
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            w,
            h,
            windowFlags);

        return m_window != nullptr;
    }

    bool sandbox_base::create_engine(std::span<const char* const> instanceExtensions,
        std::span<const char* const> instanceLayers,
        std::span<const char* const> deviceExtensions,
        void* deviceFeaturesList,
        const VkPhysicalDeviceFeatures2* physicalDeviceFeatures)
    {
        OBLO_PROFILE_SCOPE();

        // We need to gather the extensions needed by SDL
        constexpr u32 extensionsArraySize{64};
        constexpr u32 layersArraySize{16};

        buffered_array<const char*, extensionsArraySize> extensions;
        buffered_array<const char*, layersArraySize> layers;

        u32 sdlExtensionsCount;

        if (!SDL_Vulkan_GetInstanceExtensions(m_window, &sdlExtensionsCount, nullptr))
        {
            return false;
        }

        extensions.resize(sdlExtensionsCount);
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        if (!SDL_Vulkan_GetInstanceExtensions(m_window, &sdlExtensionsCount, extensions.data()))
        {
            return false;
        }

        constexpr VkValidationFeatureEnableEXT enabled[] = {VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT};
        VkValidationFeaturesEXT validationFeatures{};

        if (m_config.vkUseValidationLayers)
        {
            layers.emplace_back("VK_LAYER_KHRONOS_validation");
            extensions.emplace_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);

            validationFeatures = {
                .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
                .enabledValidationFeatureCount = array_size(enabled),
                .pEnabledValidationFeatures = enabled,
            };
        }

        extensions.insert(extensions.end(), instanceExtensions.begin(), instanceExtensions.end());
        layers.insert(layers.end(), instanceLayers.begin(), instanceLayers.end());

        constexpr u32 apiVersion{VK_API_VERSION_1_3};

        if (!m_instance.init(
                VkApplicationInfo{
                    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                    .pNext = nullptr,
                    .pApplicationName = m_config.appName,
                    .applicationVersion = 0,
                    .pEngineName = m_config.appName,
                    .engineVersion = 0,
                    .apiVersion = apiVersion,
                },
                {layers},
                {extensions},
                m_config.vkUseValidationLayers ? &validationFeatures : nullptr))
        {
            return false;
        }

        if (!SDL_Vulkan_CreateSurface(m_window, m_instance.get(), &m_surface))
        {
            return false;
        }

        create_debug_callbacks();

        constexpr const char* internalDeviceExtensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        };

        extensions.assign(std::begin(internalDeviceExtensions), std::end(internalDeviceExtensions));
        extensions.insert(extensions.end(), deviceExtensions.begin(), deviceExtensions.end());

        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .pNext = deviceFeaturesList,
            .dynamicRendering = VK_TRUE,
        };

        VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        };

        if (physicalDeviceFeatures)
        {
            physicalDeviceFeatures2 = *physicalDeviceFeatures;
        }

        physicalDeviceFeatures2.pNext = &dynamicRenderingFeature;

        return m_engine.init(m_instance.get(), m_surface, {}, {extensions}, &physicalDeviceFeatures2, nullptr) &&
            m_allocator.init(m_instance.get(), m_engine.get_physical_device(), m_engine.get_device()) &&
            m_context.init({
                .instance = m_instance.get(),
                .engine = m_engine,
                .allocator = m_allocator,
                .resourceManager = m_resourceManager,
                .buffersPerFrame =
                    2, // This number includes the buffer for incomplete transition, so it's effectively half
                .submitsInFlight = SwapchainImages,
            });
    }

    bool sandbox_base::create_swapchain()
    {
        OBLO_PROFILE_SCOPE();

        if (!m_swapchain.create(m_context, m_surface, m_renderWidth, m_renderHeight, SwapchainFormat))
        {
            return false;
        }

        const auto count = m_swapchain.get_image_count();

        // TODO: Should get this stuff from the swapchain class instead
        const image_initializer initializer{
            .imageType = VK_IMAGE_TYPE_2D,
            .format = SwapchainFormat,
            .extent = VkExtent3D{.width = m_renderWidth, .height = m_renderHeight, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .memoryUsage = memory_usage::gpu_only,
        };

        for (u32 i = 0; i < count; ++i)
        {
            m_swapchainTextures[i] = m_resourceManager.register_texture(
                texture{
                    .image = m_swapchain.get_image(i),
                    .view = m_swapchain.get_image_view(i),
                    .initializer = initializer,
                },
                VK_IMAGE_LAYOUT_UNDEFINED);
        }

        return true;
    }

    bool sandbox_base::create_synchronization_objects()
    {
        constexpr VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        string_builder nameBuilder;

        const auto debugUtilsObject = m_allocator.get_object_debug_utils();

        for (u32 i = 0; i < SwapchainImages; ++i)
        {
            if (vkCreateSemaphore(m_engine.get_device(),
                    &semaphoreInfo,
                    m_allocator.get_allocation_callbacks(),
                    &m_acquiredImages[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_engine.get_device(),
                    &semaphoreInfo,
                    m_allocator.get_allocation_callbacks(),
                    &m_frameCompleted[i]) != VK_SUCCESS)
            {
                return false;
            }

            debugUtilsObject.set_object_name(m_engine.get_device(),
                m_acquiredImages[i],
                nameBuilder.clear().format("{}[{}]", OBLO_STRINGIZE(sandbox_base::m_acquiredImages), i).c_str());

            debugUtilsObject.set_object_name(m_engine.get_device(),
                m_frameCompleted[i],
                nameBuilder.clear().format("{}[{}]", OBLO_STRINGIZE(sandbox_base::m_frameCompleted), i).c_str());
        }

        return true;
    }

    bool sandbox_base::init_imgui()
    {
        bool success = m_imgui.fill_init_command_buffer(m_window,
            m_instance.get(),
            m_engine.get_physical_device(),
            m_engine.get_device(),
            m_engine.get_queue(),
            SwapchainImages,
            SwapchainFormat,
            m_config);

        return success;
    }

    void sandbox_base::destroy_swapchain()
    {
        m_swapchain.destroy(m_context);

        for (auto& handle : m_swapchainTextures)
        {
            if (handle)
            {
                m_resourceManager.unregister_texture(handle);
                handle = {};
            }
        }
    }

    void sandbox_base::create_debug_callbacks()
    {
        // NOTE: The default allocator is used on purpose here, so we can log before creating it

        // VkDebugUtilsMessengerEXT
        {
            const PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
                reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(m_instance.get(), "vkCreateDebugUtilsMessengerEXT"));

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
                    vkCreateDebugUtilsMessengerEXT(m_instance.get(), &createInfo, nullptr, &m_vkMessenger);

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

    void sandbox_base::destroy_debug_callbacks()
    {
        if (m_vkMessenger)
        {
            const PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
                reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(m_instance.get(), "vkDestroyDebugUtilsMessengerEXT"));

            vkDestroyDebugUtilsMessengerEXT(m_instance.get(), m_vkMessenger, nullptr);
            m_vkMessenger = {};
        }
    }

    bool sandbox_base::handle_window_events(const SDL_Event& event)
    {
        if (SDL_GetWindowID(m_window) != event.window.windowID)
        {
            return true;
        }

        switch (event.window.event)
        {
        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            m_minimized = false;
            break;

        case SDL_WINDOWEVENT_MINIMIZED:
            m_minimized = true;
            break;

        case SDL_WINDOWEVENT_RESIZED:
        case SDL_WINDOWEVENT_SIZE_CHANGED: {
            OBLO_PROFILE_SCOPE("Resize swapchain");

            const u32 renderWidth = u32(event.window.data1);
            const u32 renderHeight = u32(event.window.data2);

            if (m_renderWidth != renderWidth || m_renderHeight != renderHeight)
            {
                wait_idle();

                destroy_swapchain();

                m_renderWidth = renderWidth;
                m_renderHeight = renderHeight;

                if (!create_swapchain())
                {
                    return false;
                }
            }
        }

        break;

        case SDL_WINDOWEVENT_CLOSE:
            return false;

            break;
        }

        return true;
    }
}
