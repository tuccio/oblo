#include <oblo/core/types.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/instance.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <span>
#include <vector>

enum class error
{
    success,
    create_window,
    create_surface,
    create_device,
    create_swapchain,
    create_command_buffers
};

namespace
{
    int run(SDL_Window* window)
    {
        using namespace oblo;

        constexpr u32 swapchainImages{2u};

        VkSurfaceKHR surface{nullptr};
        vk::instance instance;
        vk::single_queue_engine engine;
        vk::command_buffer_pool pool;

        {
            // We need to gather the extensions needed by SDL, for now we hardcode a max number
            constexpr u32 maxExtensionsCount{64};
            const char* vkExtensions[maxExtensionsCount];

            u32 count = maxExtensionsCount;

            if (!SDL_Vulkan_GetInstanceExtensions(window, &count, vkExtensions))
            {
                return int(error::create_device);
            }

            constexpr u32 apiVersion{VK_API_VERSION_1_0};
            constexpr const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

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
                    {},
                    {vkExtensions, count}))
            {
                return int(error::create_device);
            }

            if (!SDL_Vulkan_CreateSurface(window, instance.get(), &surface))
            {
                return int(error::create_surface);
            }

            if (!engine.init(instance.get(), surface, {}, deviceExtensions))
            {
                return int(error::create_device);
            }

            int width, height;
            SDL_Vulkan_GetDrawableSize(window, &width, &height);

            if (!engine.create_swapchain(surface, u32(width), u32(height), VK_FORMAT_B8G8R8A8_UNORM, swapchainImages))
            {
                return int(error::create_swapchain);
            }

            if (!pool.init(engine.get_device(), engine.get_queue_family_index(), false, 1, swapchainImages))
            {
                return int(error::create_command_buffers);
            }
        }

        u64 frameIndex{0};
        for (SDL_Event event;; ++frameIndex)
        {
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                case SDL_QUIT:
                    return int(error::success);
                }
            }

            pool.begin_frame(frameIndex);

            VkCommandBuffer commandBuffer;
            pool.fetch_buffers({&commandBuffer, 1});

            const VkCommandBufferBeginInfo commandBufferbBeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                                   .pNext = nullptr,
                                                                   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                                   .pInheritanceInfo = nullptr};

            vkBeginCommandBuffer(commandBuffer, &commandBufferbBeginInfo);
            vkEndCommandBuffer(commandBuffer);
        }

        if (surface)
        {
            vkDestroySurfaceKHR(instance.get(), surface, nullptr);
        }
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