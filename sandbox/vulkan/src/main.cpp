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
    create_command_buffers
};

namespace
{
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
        vk::command_buffer_pool pool;

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