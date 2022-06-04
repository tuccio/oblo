#include <oblo/core/types.hpp>
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
    create_vulkan_context,
};

namespace
{
    int run(SDL_Window* window)
    {
        using namespace oblo;

        vk::instance instance;
        vk::single_queue_engine engine;

        {
            // We need to gather the extensions needed by SDL, for now we hardcode a max number
            constexpr u32 maxExtensionsCount{64};
            const char* vkExtensions[maxExtensionsCount];

            u32 count = maxExtensionsCount;

            if (!SDL_Vulkan_GetInstanceExtensions(window, &count, vkExtensions))
            {
                return int(error::create_vulkan_context);
            }

            constexpr u32 apiVersion{VK_API_VERSION_1_0};
            constexpr const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

            if (!instance.init(
                    VkApplicationInfo{
                        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                        .pNext = nullptr,
                        .pApplicationName = "oblo",
                        .applicationVersion = 0,
                        .pEngineName = "oblo",
                        .engineVersion = 0,
                        .apiVersion = apiVersion,
                    },
                    {},
                    {vkExtensions, count}))
            {
                return int(error::create_vulkan_context);
            }

            VkSurfaceKHR surface;

            if (!SDL_Vulkan_CreateSurface(window, instance.get(), &surface))
            {
                return int(error::create_surface);
            }

            if (!engine.init(instance.get(), surface, {}, deviceExtensions))
            {
                return int(error::create_vulkan_context);
            }

            int width, height;
            SDL_Vulkan_GetDrawableSize(window, &width, &height);

            if (!engine.create_swapchain(surface, u32(width), u32(height), VK_FORMAT_B8G8R8A8_UNORM, 2u))
            {
                return false;
            }
        }

        for (SDL_Event event;;)
        {
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                case SDL_QUIT:
                    return int(error::success);
                }
            }
        }
    }
}

int SDL_main(int, char*[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    auto* const window = SDL_CreateWindow("SDL Vulkan Sample",
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