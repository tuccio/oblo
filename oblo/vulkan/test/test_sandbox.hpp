#pragma once

#include <oblo/core/small_vector.hpp>
#include <oblo/vulkan/instance.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan.h>

#include <span>
#include <sstream>

#define ASSERT_VK_SUCCESS(...) ASSERT_EQ(VK_SUCCESS, __VA_ARGS__)

namespace oblo::vk
{
    inline VKAPI_ATTR VkBool32 VKAPI_CALL
    validation_cb([[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
                  [[maybe_unused]] const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  void* pUserData)
    {
        auto& errors = *static_cast<std::stringstream*>(pUserData);

        errors << "[Vulkan Validation] (" << std::hex << messageType << ") " << pCallbackData->pMessage;
        return VK_FALSE;
    }

    class test_sandbox
    {
    public:
        test_sandbox() = default;
        test_sandbox(const test_sandbox&) = delete;
        test_sandbox(test_sandbox&&) noexcept = delete;
        test_sandbox& operator=(const test_sandbox&) = delete;
        test_sandbox& operator=(test_sandbox&&) noexcept = delete;

        ~test_sandbox()
        {
            shutdown();
        }

        bool init(std::span<const char* const> instanceExtensions,
                  std::span<const char* const> instanceLayers,
                  std::span<const char* const> deviceExtensions,
                  void* deviceFeaturesList,
                  const VkPhysicalDeviceFeatures* physicalDeviceFeatures,
                  std::stringstream* validationErrors)
        {
            if (!create_window() || !create_engine(instanceExtensions,
                                                   instanceLayers,
                                                   deviceExtensions,
                                                   deviceFeaturesList,
                                                   physicalDeviceFeatures,
                                                   validationErrors))
            {
                return false;
            }

            return true;
        }

        void shutdown()
        {
            if (surface)
            {
                vkDestroySurfaceKHR(instance.get(), surface, nullptr);
            }

            if (window)
            {
                SDL_DestroyWindow(window);
                window = nullptr;
            }

            engine.shutdown();
            instance.shutdown();
        }

    private:
        bool create_window()
        {
            window = SDL_CreateWindow("Oblo Vulkan Test",
                                      SDL_WINDOWPOS_UNDEFINED,
                                      SDL_WINDOWPOS_UNDEFINED,
                                      1280,
                                      720,
                                      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

            return window != nullptr;
        }

        bool create_engine(std::span<const char* const> instanceExtensions,
                           std::span<const char* const> instanceLayers,
                           std::span<const char* const> deviceExtensions,
                           void* deviceFeaturesList,
                           const VkPhysicalDeviceFeatures* physicalDeviceFeatures,
                           std::stringstream* validationErrors)
        {
            // We need to gather the extensions needed by SDL
            constexpr u32 extensionsArraySize{64};
            constexpr u32 layersArraySize{16};

            small_vector<const char*, extensionsArraySize> extensions;
            small_vector<const char*, layersArraySize> layers;

            u32 sdlExtensionsCount;

            if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionsCount, nullptr))
            {
                return false;
            }

            extensions.resize(sdlExtensionsCount);

            if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionsCount, extensions.data()))
            {
                return false;
            }

            layers.emplace_back("VK_LAYER_KHRONOS_validation");

            extensions.insert(extensions.end(), instanceExtensions.begin(), instanceExtensions.end());
            layers.insert(layers.end(), instanceLayers.begin(), instanceLayers.end());

            constexpr u32 apiVersion{VK_API_VERSION_1_3};

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
                    {layers},
                    {extensions},
                    validation_cb,
                    validationErrors))
            {
                return false;
            }

            if (!SDL_Vulkan_CreateSurface(window, instance.get(), &surface))
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

            return engine
                .init(instance.get(), surface, {}, {extensions}, &bufferDeviceAddressFeature, physicalDeviceFeatures);
        }

    public:
        SDL_Window* window;
        VkSurfaceKHR surface{nullptr};

        instance instance;
        single_queue_engine engine;
    };
}