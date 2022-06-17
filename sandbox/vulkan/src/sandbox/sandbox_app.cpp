#include <sandbox/sandbox_app.hpp>

#include <oblo/core/array_size.hpp>
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
        debugCallback([[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
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

        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }
    }

    bool sandbox_base::init(std::span<const char* const> deviceExtensions,
                            void* deviceFeaturesList,
                            const VkPhysicalDeviceFeatures* physicalDeviceFeatures)
    {
        load_config();

        if (!create_window() || !create_engine(deviceExtensions, deviceFeaturesList, physicalDeviceFeatures) ||
            !m_allocator.init(m_instance.get(), m_engine.get_physical_device(), m_engine.get_device()))
        {
            return false;
        }

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

                    m_swapchain.destroy(m_engine);

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

    void sandbox_base::begin_frame(u64 frameIndex,
                                   u32* outImageIndex,
                                   u32* outPoolIndex,
                                   VkCommandBuffer* outCommandBuffer)
    {
        const auto poolIndex = frameIndex % SwapchainImages;

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

        pool.reset_pool();
        pool.begin_frame(frameIndex);

        const VkCommandBuffer commandBuffer = pool.fetch_buffer();

        const VkCommandBufferBeginInfo commandBufferBeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                              .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

        OBLO_VK_PANIC(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

        *outImageIndex = imageIndex;
        *outPoolIndex = poolIndex;
        *outCommandBuffer = commandBuffer;
    }

    void sandbox_base::submit_and_present(VkCommandBuffer commandBuffer, u32 imageIndex, u32 poolIndex, u64 frameIndex)
    {
        {
            const VkImageMemoryBarrier imageMemoryBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                          .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                          .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                          .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                          .image = m_swapchain.get_image(imageIndex),
                                                          .subresourceRange = {
                                                              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                              .baseMipLevel = 0,
                                                              .levelCount = 1,
                                                              .baseArrayLayer = 0,
                                                              .layerCount = 1,
                                                          }};

            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &imageMemoryBarrier);
        }

        OBLO_VK_PANIC(vkEndCommandBuffer(commandBuffer));

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
                                      .commandBufferCount = 1,
                                      .pCommandBuffers = &commandBuffer,
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

    bool sandbox_base::create_engine(std::span<const char* const> deviceExtensions,
                                     void* deviceFeaturesList,
                                     const VkPhysicalDeviceFeatures* physicalDeviceFeatures)
    {
        // We need to gather the extensions needed by SDL, for now we hardcode a max number
        constexpr u32 extensionsArraySize{64};
        constexpr u32 layersArraySize{16};
        const char* extensions[extensionsArraySize];
        const char* layers[layersArraySize];

        u32 extensionsCount = extensionsArraySize;
        u32 layersCount = 0;

        if (!SDL_Vulkan_GetInstanceExtensions(m_window, &extensionsCount, extensions))
        {
            return false;
        }

        if (m_config.vk_use_validation_layers)
        {
            layers[layersCount++] = "VK_LAYER_KHRONOS_validation";
        }

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
                {layers, layersCount},
                {extensions, extensionsCount},
                debugCallback))
        {
            return false;
        }

        if (!SDL_Vulkan_CreateSurface(m_window, m_instance.get(), &m_surface))
        {
            return false;
        }

        constexpr const char* internalDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                            VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
                                                            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME};

        // TODO: Could heap allocate instead
        if (array_size(internalDeviceExtensions) + deviceExtensions.size() > extensionsArraySize)
        {
            return false;
        }

        const char* deviceExtensionsArray[extensionsArraySize];
        auto deviceExtensionsArrayEnd = deviceExtensionsArray;

        for (auto* extension : internalDeviceExtensions)
        {
            *deviceExtensionsArrayEnd = extension;
            ++deviceExtensionsArrayEnd;
        }

        for (auto* extension : deviceExtensions)
        {
            *deviceExtensionsArrayEnd = extension;
            ++deviceExtensionsArrayEnd;
        }

        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .pNext = deviceFeaturesList,
            .dynamicRendering = VK_TRUE};

        VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeature{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
            .pNext = &dynamicRenderingFeature,
            .timelineSemaphore = VK_TRUE};

        return m_engine.init(m_instance.get(),
                             m_surface,
                             {},
                             std::span{deviceExtensionsArray, deviceExtensionsArrayEnd},
                             &timelineFeature,
                             physicalDeviceFeatures);
    }

    bool sandbox_base::create_swapchain()
    {
        return m_swapchain.create(m_engine, m_surface, m_renderWidth, m_renderHeight, SwapchainFormat);
    }

    bool sandbox_base::create_command_pools()
    {
        for (auto& pool : m_pools)
        {
            if (!pool.init(m_engine.get_device(), m_engine.get_queue_family_index(), false, 1, 1))
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

        pool.reset_buffers(frameIndex);
        pool.reset_pool();

        return result;
    }
}