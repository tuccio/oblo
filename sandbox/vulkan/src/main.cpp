#include <oblo/core/types.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/instance.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

enum class error
{
    success,
    create_window,
    create_surface,
    create_device,
    create_swapchain,
    create_command_buffers,
    create_pipeline
};

namespace
{
    struct extensions
    {
        PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
        PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;
    };

    [[maybe_unused]] extensions load_extensions(VkInstance instance)
    {
        auto vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());

        extensions res{};

        res.vkCmdBeginRenderingKHR =
            reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetInstanceProcAddr(instance, "vkCmdBeginRenderingKHR"));

        res.vkCmdEndRenderingKHR =
            reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetInstanceProcAddr(instance, "vkCmdEndRenderingKHR"));

        return res;
    }

    struct config
    {
        bool vk_use_validation_layers{false};
    };

#define OBLO_READ_VAR_(Var)                                                                                            \
    if (json.count(#Var) > 0)                                                                                          \
        cfg.Var = json.at(#Var).get<decltype(config::Var)>();

    config load_config(const std::filesystem::path& path)
    {
        config cfg{};
        std::ifstream ifs{path};

        if (ifs.is_open())
        {
            const auto json = nlohmann::json::parse(ifs);
            OBLO_READ_VAR_(vk_use_validation_layers);
        }

        return cfg;
    }
#undef OBLO_READ_VAR_

    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback([[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  [[maybe_unused]] void* pUserData)
    {
        fprintf(stderr, "[Vulkan Validation] (%x) %s\n", messageType, pCallbackData->pMessage);
        return VK_FALSE;
    }

    int run(SDL_Window* window)
    {
        using namespace oblo;

        const auto config = load_config("vksandbox.json");

        constexpr u32 swapchainImages{2u};

        vk::instance instance;
        vk::single_queue_engine engine;
        vk::command_buffer_pool pools[swapchainImages];

        u32 renderWidth, renderHeight;

        {
            // We need to gather the extensions needed by SDL, for now we hardcode a max number
            constexpr u32 extensionsArraySize{64};
            constexpr u32 layersArraySize{16};
            const char* extensions[extensionsArraySize];
            const char* layers[layersArraySize];

            u32 extensionsCount = extensionsArraySize;
            u32 layersCount = 0;

            if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionsCount, extensions))
            {
                return int(error::create_device);
            }

            if (config.vk_use_validation_layers)
            {
                layers[layersCount++] = "VK_LAYER_KHRONOS_validation";
            }

            constexpr u32 apiVersion{VK_API_VERSION_1_2};
            constexpr const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
                                                        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME};

            if (!instance.init(
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
                return int(error::create_device);
            }

            VkSurfaceKHR surface{nullptr};

            if (!SDL_Vulkan_CreateSurface(window, instance.get(), &surface))
            {
                return int(error::create_surface);
            }

            const VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeature{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
                .pNext = nullptr,
                .timelineSemaphore = VK_TRUE};

            if (!engine.init(instance.get(), std::move(surface), {}, deviceExtensions, &timelineFeature))
            {
                return int(error::create_device);
            }

            int width, height;
            SDL_Vulkan_GetDrawableSize(window, &width, &height);

            renderWidth = u32(width);
            renderHeight = u32(height);

            if (!engine.create_swapchain(surface, renderWidth, renderHeight, VK_FORMAT_B8G8R8A8_UNORM, swapchainImages))
            {
                return int(error::create_swapchain);
            }

            for (auto& pool : pools)
            {
                if (!pool.init(engine.get_device(), engine.get_queue_family_index(), false, 1, 1))
                {
                    return int(error::create_command_buffers);
                }
            }
        }

        VkSemaphore presentSemaphore;

        {
            const VkSemaphoreCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                   .pNext = nullptr,
                                                   .flags = 0};

            OBLO_VK_PANIC(vkCreateSemaphore(engine.get_device(), &createInfo, nullptr, &presentSemaphore));
        }

        VkSemaphore timelineSemaphore;

        {
            const VkSemaphoreTypeCreateInfo timelineCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                                               .pNext = nullptr,
                                                               .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                                                               .initialValue = 0};

            const VkSemaphoreCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                   .pNext = &timelineCreateInfo,
                                                   .flags = 0};

            OBLO_VK_PANIC(vkCreateSemaphore(engine.get_device(), &createInfo, nullptr, &timelineSemaphore));
        }

        VkFence presentFences[2];

        {
            const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                              .pNext = nullptr,
                                              .flags = 0u};

            OBLO_VK_PANIC(vkCreateFence(engine.get_device(), &fenceInfo, nullptr, &presentFences[0]));
            OBLO_VK_PANIC(vkCreateFence(engine.get_device(), &fenceInfo, nullptr, &presentFences[1]));
        }

        u64 frameSemaphoreValues[swapchainImages] = {0};

        u64 currentSemaphoreValue{0};

        for (u64 frameIndex{1};; ++frameIndex)
        {
            for (SDL_Event event; SDL_PollEvent(&event);)
            {
                switch (event.type)
                {
                case SDL_QUIT:
                    goto done;
                }
            }

            const auto currentIndex = frameIndex % swapchainImages;

            OBLO_VK_PANIC(vkGetSemaphoreCounterValue(engine.get_device(), timelineSemaphore, &currentSemaphoreValue));

            if (currentSemaphoreValue < frameSemaphoreValues[currentIndex])
            {
                OBLO_VK_PANIC(vkWaitForFences(engine.get_device(), 1, &presentFences[currentIndex], 0, ~0ull));
            }

            OBLO_VK_PANIC(vkResetFences(engine.get_device(), 1, &presentFences[currentIndex]));

            u32 imageIndex;

            OBLO_VK_PANIC(vkAcquireNextImageKHR(engine.get_device(),
                                                engine.get_swapchain(),
                                                UINT64_MAX,
                                                presentSemaphore,
                                                VK_NULL_HANDLE,
                                                &imageIndex));

            auto& pool = pools[currentIndex];

            pool.reset_pool();
            pool.begin_frame(frameIndex);

            VkCommandBuffer commandBuffer;
            pool.fetch_buffers({&commandBuffer, 1});

            const VkCommandBufferBeginInfo commandBufferBeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                                  .pNext = nullptr,
                                                                  .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                                  .pInheritanceInfo = nullptr};

            OBLO_VK_PANIC(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout =
                frameIndex <= swapchainImages ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = engine.get_image(imageIndex);
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = 0; // TODO
            barrier.dstAccessMask = 0; // TODO

            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT /* TODO */,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT /* TODO */,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &barrier);

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
                                          .pSignalSemaphores = &timelineSemaphore};

            OBLO_VK_PANIC(vkQueueSubmit(engine.get_queue(), 1, &submitInfo, presentFences[currentIndex]));

            auto swapchain = engine.get_swapchain();
            const VkPresentInfoKHR presentInfo = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                                  .pNext = nullptr,
                                                  .waitSemaphoreCount = 1,
                                                  .pWaitSemaphores = &presentSemaphore,
                                                  .swapchainCount = 1,
                                                  .pSwapchains = &swapchain,
                                                  .pImageIndices = &imageIndex,
                                                  .pResults = nullptr};

            OBLO_VK_PANIC(vkQueuePresentKHR(engine.get_queue(), &presentInfo));

            frameSemaphoreValues[currentIndex] = frameIndex;
        }

    done:
        vkDeviceWaitIdle(engine.get_device());

        for (auto fence : presentFences)
        {
            vkDestroyFence(engine.get_device(), fence, nullptr);
        }

        vkDestroySemaphore(engine.get_device(), timelineSemaphore, nullptr);
        vkDestroySemaphore(engine.get_device(), presentSemaphore, nullptr);

        return int(error::success);
    }
}

int SDL_main(int, char*[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    auto* const window = SDL_CreateWindow("Oblo Vulkan Sandbox",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          1280,
                                          720,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

    if (!window)
    {
        return int(error::create_window);
    }

    const auto result = run(window);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return result;
}