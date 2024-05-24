#include <oblo/sandbox/sandbox_app.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/log.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/error.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <fstream>
#include <span>
#include <vector>

namespace oblo::vk
{
    namespace
    {
        VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            [[maybe_unused]] void* pUserData)
        {
            log::severity severity = log::severity::debug;

            switch (messageSeverity)
            {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                severity = log::severity::info;
                break;

            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                severity = log::severity::warn;
                break;

            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                severity = log::severity::error;
                break;

            default:
                break;
            }

            log::generic(severity, "[Vulkan] (0x{:x}) {}", messageType, pCallbackData->pMessage);
            return VK_FALSE;
        }
    }

    void sandbox_base::shutdown()
    {
        if (m_engine.get_device())
        {
            m_imgui.shutdown(m_engine.get_device());

            m_context.reset_immediate(m_acquiredImage);
            m_context.reset_immediate(m_frameCompleted);

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
        vkDeviceWaitIdle(m_engine.get_device());
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

        timestamp sdl_convert_time(u32 time)
        {
            // TODO: Convert from ms to our unit (maybe 100 ns?)
            return time;
        }
    }

    bool sandbox_base::poll_events()
    {
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
                        .time = sdl_convert_time(event.button.timestamp),
                        .mousePress =
                            {
                                .key = sdl_map_mouse_key(event.button.button),
                            },
                    });
                    break;

                case SDL_MOUSEBUTTONUP:
                    m_inputQueue.push({
                        .kind = input_event_kind::mouse_release,
                        .time = sdl_convert_time(event.button.timestamp),
                        .mouseRelease =
                            {
                                .key = sdl_map_mouse_key(event.button.button),
                            },
                    });
                    break;

                case SDL_MOUSEMOTION:
                    m_inputQueue.push({
                        .kind = input_event_kind::mouse_move,
                        .time = sdl_convert_time(event.motion.timestamp),
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
                        .time = sdl_convert_time(event.key.timestamp),
                        .keyboardPress =
                            {
                                .key = sdl_map_keyboard_key(event.key.keysym.sym),
                            },
                    });
                    break;

                case SDL_KEYUP:
                    m_inputQueue.push({
                        .kind = input_event_kind::keyboard_release,
                        .time = sdl_convert_time(event.key.timestamp),
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
        // We could submit the semaphore to wait on here, if everyone submitted through the context
        m_context.frame_begin(nullptr);

        u32 imageIndex;

        VkResult acquireImageResult;

        do
        {
            acquireImageResult = vkAcquireNextImageKHR(m_engine.get_device(),
                m_swapchain.get(),
                UINT64_MAX,
                m_acquiredImage,
                VK_NULL_HANDLE,
                &imageIndex);

            if (acquireImageResult == VK_SUCCESS)
            {
                //// Make sure we wait for the acquired image for any future submission, not optimal, it would be better
                //// to piggy-back to the next submission, but we submit in different places (e.g. staging_buffer)

                // constexpr VkPipelineStageFlags submitPipelineStages[] =
                // {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

                // const VkSubmitInfo submitInfo{
                //     .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                //     .waitSemaphoreCount = 1,
                //     .pWaitSemaphores = &m_acquiredImage,
                //     .pWaitDstStageMask = submitPipelineStages,
                // };

                // OBLO_VK_PANIC(vkQueueSubmit(m_engine.get_queue(), 1, &submitInfo, nullptr));

                //vkDeviceWaitIdle(m_engine.get_device());

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
        m_context.frame_end();

        // Again, not ideal, just submit to signal the render being completed, so present can wait on it
        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &m_frameCompleted,
        };

        OBLO_VK_PANIC(vkQueueSubmit(m_engine.get_queue(), 1, &submitInfo, nullptr));

        /*  const VkTimelineSemaphoreSubmitInfo timelineInfo{
              .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
              .pNext = nullptr,
              .waitSemaphoreValueCount = 0,
              .pWaitSemaphoreValues = nullptr,
              .signalSemaphoreValueCount = 1,
              .pSignalSemaphoreValues = &submitIndex,
          };*/

        const auto swapchain = m_swapchain.get();

        const VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            //.pNext = &timelineInfo,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_frameCompleted,
            //.pWaitSemaphores = &submitSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex,
            .pResults = nullptr,
        };

        OBLO_VK_PANIC_EXCEPT(vkQueuePresentKHR(m_engine.get_queue(), &presentInfo), VK_ERROR_OUT_OF_DATE_KHR);
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

        if (!SDL_Vulkan_GetInstanceExtensions(m_window, &sdlExtensionsCount, extensions.data()))
        {
            return false;
        }

        if (m_config.vkUseValidationLayers)
        {
            layers.emplace_back("VK_LAYER_KHRONOS_validation");
            extensions.emplace_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
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
                debug_callback))
        {
            return false;
        }

        if (!SDL_Vulkan_CreateSurface(m_window, m_instance.get(), &m_surface))
        {
            return false;
        }

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

        VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeature{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
            .pNext = &dynamicRenderingFeature,
            .timelineSemaphore = VK_TRUE,
        };

        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeature{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = &timelineFeature,
            .bufferDeviceAddress = VK_TRUE,
        };

        VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};

        if (physicalDeviceFeatures)
        {
            physicalDeviceFeatures2 = *physicalDeviceFeatures;
        }

        physicalDeviceFeatures2.pNext = &bufferDeviceAddressFeature;

        return m_engine.init(m_instance.get(), m_surface, {}, {extensions}, &physicalDeviceFeatures2, nullptr) &&
            m_allocator.init(m_instance.get(), m_engine.get_physical_device(), m_engine.get_device()) &&
            m_context.init({
                .instance = m_instance.get(),
                .engine = m_engine,
                .allocator = m_allocator,
                .resourceManager = m_resourceManager,
                .buffersPerFrame = 2,
                .submitsInFlight = SwapchainImages,
            });
    }

    bool sandbox_base::create_swapchain()
    {
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
        const VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        if (vkCreateSemaphore(m_engine.get_device(),
                &semaphoreInfo,
                m_allocator.get_allocation_callbacks(),
                &m_acquiredImage) != VK_SUCCESS ||
            vkCreateSemaphore(m_engine.get_device(),
                &semaphoreInfo,
                m_allocator.get_allocation_callbacks(),
                &m_frameCompleted) != VK_SUCCESS)
        {
            return false;
        }

        const auto debugUtilsObject = m_allocator.get_object_debug_utils();

        debugUtilsObject.set_object_name(m_engine.get_device(),
            m_acquiredImage,
            OBLO_STRINGIZE(sandbox_base::m_acquiredImage));

        debugUtilsObject.set_object_name(m_engine.get_device(),
            m_frameCompleted,
            OBLO_STRINGIZE(sandbox_base::m_frameCompleted));

        return true;
    }

    bool sandbox_base::init_imgui()
    {
        m_context.frame_begin(nullptr);

        auto& commandBuffer = m_context.get_active_command_buffer();

        bool success = m_imgui.fill_init_command_buffer(m_window,
            m_instance.get(),
            m_engine.get_physical_device(),
            m_engine.get_device(),
            m_engine.get_queue(),
            commandBuffer.get(),
            SwapchainImages,
            m_config);

        m_context.frame_end();

        if (success)
        {
            m_imgui.finalize_init(m_engine.get_device());
        }

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

    bool sandbox_base::handle_window_events(const SDL_Event& event)
    {
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
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            wait_idle();

            destroy_swapchain();

            m_renderWidth = u32(event.window.data1);
            m_renderHeight = u32(event.window.data2);

            if (!create_swapchain())
            {
                return false;
            }

            break;

        case SDL_WINDOWEVENT_CLOSE:
            if (SDL_GetWindowID(m_window) == event.window.windowID)
            {
                return false;
            }

            break;
        }

        return true;
    }
}
