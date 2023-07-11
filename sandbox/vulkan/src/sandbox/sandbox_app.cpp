#include <sandbox/sandbox_app.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/small_vector.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/error.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <fstream>
#include <span>
#include <vector>

#include <nlohmann/json.hpp>

#define OBLO_READ_CFG_VAR(Config, Var)                                                                                 \
    if (json.count(#Var) > 0)                                                                                          \
        Config.Var = json.at(#Var).get<decltype(config::Var)>();

namespace oblo::vk
{
    namespace
    {
        VKAPI_ATTR VkBool32 VKAPI_CALL
        debug_callback([[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                       [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
                       const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                       [[maybe_unused]] void* pUserData)
        {
            fprintf(stderr, "[Vulkan Validation] (%x) %s\n", messageType, pCallbackData->pMessage);
            return VK_FALSE;
        }
    }

    void sandbox_base::shutdown()
    {
        if (m_engine.get_device())
        {
            m_imgui.shutdown(m_engine.get_device());

            reset_device_objects(m_engine.get_device(),
                                 std::span{m_presentFences},
                                 m_timelineSemaphore,
                                 m_presentSemaphore);

            m_swapchain.destroy(m_engine);
        }

        if (m_surface)
        {
            vkDestroySurfaceKHR(m_instance.get(), m_surface, nullptr);
        }

        m_renderPassManager.shutdown();

        for (auto& pool : m_pools)
        {
            pool.shutdown();
        }

        m_allocator.shutdown();
        m_engine.shutdown();

        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        m_frameAllocator.shutdown();
    }

    bool sandbox_base::init(std::span<const char* const> instanceExtensions,
                            std::span<const char* const> instanceLayers,
                            std::span<const char* const> deviceExtensions,
                            void* deviceFeaturesList,
                            const VkPhysicalDeviceFeatures* physicalDeviceFeatures)
    {
        m_frameAllocator.init(1u << 30, 1u << 24, 1u);

        load_config();

        if (!create_window() ||
            !create_engine(instanceExtensions,
                           instanceLayers,
                           deviceExtensions,
                           deviceFeaturesList,
                           physicalDeviceFeatures) ||
            !m_allocator.init(m_instance.get(), m_engine.get_physical_device(), m_engine.get_device()))
        {
            return false;
        }

        m_renderPassManager.init(m_engine.get_device());

        int width, height;
        SDL_Vulkan_GetDrawableSize(m_window, &width, &height);

        m_renderWidth = u32(width);
        m_renderHeight = u32(height);

        if (!create_swapchain())
        {
            return false;
        }

        return create_command_pools() && create_synchronization_objects() && init_imgui();
    }

    void sandbox_base::wait_idle()
    {
        vkDeviceWaitIdle(m_engine.get_device());
    }

    bool sandbox_base::poll_events()
    {
        for (SDL_Event event; SDL_PollEvent(&event);)
        {
            switch (event.type)
            {
            case SDL_QUIT:
                return false;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    wait_idle();

                    destroy_swapchain();

                    m_renderWidth = u32(event.window.data1);
                    m_renderHeight = u32(event.window.data2);

                    if (!create_swapchain())
                    {
                        return false;
                    }
                }

                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.scancode == SDL_SCANCODE_F2)
                {
                    m_showImgui = !m_showImgui;
                }
            }

            if (m_showImgui)
            {
                m_imgui.process(event);
            }
        }

        return true;
    }

    void sandbox_base::begin_frame(u64 frameIndex, u32* outImageIndex, u32* outPoolIndex)
    {
        const auto poolIndex = u32(frameIndex % SwapchainImages);

        OBLO_VK_PANIC(vkGetSemaphoreCounterValue(m_engine.get_device(), m_timelineSemaphore, &m_currentSemaphoreValue));

        if (m_currentSemaphoreValue < m_frameSemaphoreValues[poolIndex])
        {
            OBLO_VK_PANIC(vkWaitForFences(m_engine.get_device(), 1, &m_presentFences[poolIndex], 0, ~0ull));
        }

        OBLO_VK_PANIC(vkResetFences(m_engine.get_device(), 1, &m_presentFences[poolIndex]));

        u32 imageIndex;

        OBLO_VK_PANIC(vkAcquireNextImageKHR(m_engine.get_device(),
                                            m_swapchain.get(),
                                            UINT64_MAX,
                                            m_presentSemaphore,
                                            VK_NULL_HANDLE,
                                            &imageIndex));

        auto& pool = m_pools[poolIndex];

        pool.reset_buffers(frameIndex);
        pool.reset_pool();
        pool.begin_frame(frameIndex);

        *outImageIndex = imageIndex;
        *outPoolIndex = poolIndex;
    }

    void sandbox_base::begin_command_buffers(u32 poolIndex, VkCommandBuffer* outCommandBuffers, u32 count)
    {
        auto& pool = m_pools[poolIndex];
        pool.fetch_buffers({outCommandBuffers, count});

        constexpr VkCommandBufferBeginInfo commandBufferBeginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        for (auto* it = outCommandBuffers; it != outCommandBuffers + count; ++it)
        {
            OBLO_VK_PANIC(vkBeginCommandBuffer(*it, &commandBufferBeginInfo));
        }
    }

    void sandbox_base::end_command_buffers(const VkCommandBuffer* commandBuffers, u32 count)
    {
        for (auto* it = commandBuffers; it != commandBuffers + count; ++it)
        {
            OBLO_VK_PANIC(vkEndCommandBuffer(*it));
        }
    }

    void sandbox_base::submit_and_present(
        const VkCommandBuffer* commandBuffers, u32 commandBuffersCount, u32 imageIndex, u32 poolIndex, u64 frameIndex)
    {
        const VkTimelineSemaphoreSubmitInfo timelineInfo{.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
                                                         .pNext = nullptr,
                                                         .waitSemaphoreValueCount = 0,
                                                         .pWaitSemaphoreValues = nullptr,
                                                         .signalSemaphoreValueCount = 1,
                                                         .pSignalSemaphoreValues = &frameIndex};

        const VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                      .pNext = &timelineInfo,
                                      .waitSemaphoreCount = 0,
                                      .pWaitSemaphores = nullptr,
                                      .commandBufferCount = commandBuffersCount,
                                      .pCommandBuffers = commandBuffers,
                                      .signalSemaphoreCount = 1,
                                      .pSignalSemaphores = &m_timelineSemaphore};

        OBLO_VK_PANIC(vkQueueSubmit(m_engine.get_queue(), 1, &submitInfo, m_presentFences[poolIndex]));

        const auto swapchain = m_swapchain.get();
        const VkPresentInfoKHR presentInfo = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                              .pNext = nullptr,
                                              .waitSemaphoreCount = 1,
                                              .pWaitSemaphores = &m_presentSemaphore,
                                              .swapchainCount = 1,
                                              .pSwapchains = &swapchain,
                                              .pImageIndices = &imageIndex,
                                              .pResults = nullptr};

        OBLO_VK_PANIC(vkQueuePresentKHR(m_engine.get_queue(), &presentInfo));

        m_frameSemaphoreValues[poolIndex] = frameIndex;
    }

    void sandbox_base::load_config()
    {
        m_config = {};
        std::ifstream ifs{"vksandbox.json"};

        if (ifs.is_open())
        {
            const auto json = nlohmann::json::parse(ifs);
            OBLO_READ_CFG_VAR(m_config, vk_use_validation_layers);
        }
    }

    bool sandbox_base::create_window()
    {
        m_window = SDL_CreateWindow("Oblo Vulkan Sandbox",
                                    SDL_WINDOWPOS_UNDEFINED,
                                    SDL_WINDOWPOS_UNDEFINED,
                                    1280,
                                    720,
                                    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

        return m_window != nullptr;
    }

    bool sandbox_base::create_engine(std::span<const char* const> instanceExtensions,
                                     std::span<const char* const> instanceLayers,
                                     std::span<const char* const> deviceExtensions,
                                     void* deviceFeaturesList,
                                     const VkPhysicalDeviceFeatures* physicalDeviceFeatures)
    {
        // We need to gather the extensions needed by SDL
        constexpr u32 extensionsArraySize{64};
        constexpr u32 layersArraySize{16};

        small_vector<const char*, extensionsArraySize> extensions;
        small_vector<const char*, layersArraySize> layers;

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

        if (m_config.vk_use_validation_layers)
        {
            layers.emplace_back("VK_LAYER_KHRONOS_validation");
        }

        extensions.insert(extensions.end(), instanceExtensions.begin(), instanceExtensions.end());
        layers.insert(layers.end(), instanceLayers.begin(), instanceLayers.end());

        constexpr u32 apiVersion{VK_API_VERSION_1_3};

        if (!m_instance.init(
                VkApplicationInfo{
                    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                    .pNext = nullptr,
                    .pApplicationName = "vksandbox",
                    .applicationVersion = 0,
                    .pEngineName = "oblo",
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

        return m_engine
            .init(m_instance.get(), m_surface, {}, {extensions}, &bufferDeviceAddressFeature, physicalDeviceFeatures);
    }

    bool sandbox_base::create_swapchain()
    {
        if (!m_swapchain.create(m_engine, m_surface, m_renderWidth, m_renderHeight, SwapchainFormat))
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

    bool sandbox_base::create_command_pools()
    {
        // Kind of arbitrary count for the pool, could implement growth instead
        constexpr u32 buffersPerFrame{16};

        for (auto& pool : m_pools)
        {
            if (!pool.init(m_engine.get_device(), m_engine.get_queue_family_index(), false, buffersPerFrame, 1))
            {
                return false;
            }
        }

        return true;
    }

    bool sandbox_base::create_synchronization_objects()
    {
        const VkSemaphoreCreateInfo presentSemaphoreCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                               .pNext = nullptr,
                                                               .flags = 0};

        const VkSemaphoreTypeCreateInfo timelineTypeCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                                               .pNext = nullptr,
                                                               .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                                                               .initialValue = 0};

        const VkSemaphoreCreateInfo timelineSemaphoreCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                                .pNext = &timelineTypeCreateInfo,
                                                                .flags = 0};

        const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0u};

        for (u32 i = 0; i < SwapchainImages; ++i)
        {
            if (vkCreateFence(m_engine.get_device(), &fenceInfo, nullptr, &m_presentFences[i]) != VK_SUCCESS)
            {
                return false;
            }
        }

        return vkCreateSemaphore(m_engine.get_device(), &presentSemaphoreCreateInfo, nullptr, &m_presentSemaphore) ==
                   VK_SUCCESS &&
               vkCreateSemaphore(m_engine.get_device(), &timelineSemaphoreCreateInfo, nullptr, &m_timelineSemaphore) ==
                   VK_SUCCESS;
    }

    bool sandbox_base::init_imgui()
    {
        auto& pool = m_pools[0];
        constexpr auto frameIndex{0u};
        pool.begin_frame(frameIndex);

        const auto commandBuffer = pool.fetch_buffer();

        bool result = m_imgui.init(m_window,
                                   m_instance.get(),
                                   m_engine.get_physical_device(),
                                   m_engine.get_device(),
                                   m_engine.get_queue(),
                                   commandBuffer,
                                   SwapchainImages);

        pool.reset_buffers(frameIndex + 1);
        pool.reset_pool();

        return result;
    }

    void sandbox_base::destroy_swapchain()
    {
        m_swapchain.destroy(m_engine);

        for (auto& handle : m_swapchainTextures)
        {
            if (handle)
            {
                m_resourceManager.unregister_texture(handle);
                handle = {};
            }
        }
    }
}
