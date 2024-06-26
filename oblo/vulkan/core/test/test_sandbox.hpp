#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/vulkan/instance.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>

#include <vulkan/vulkan.h>

#include <span>
#include <sstream>

#define ASSERT_VK_SUCCESS(...) ASSERT_EQ(VK_SUCCESS, __VA_ARGS__)

namespace oblo::vk
{
    inline VKAPI_ATTR VkBool32 VKAPI_CALL validation_cb(
        [[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
        [[maybe_unused]] const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            auto& errors = *static_cast<std::stringstream*>(pUserData);
            errors << "[Vulkan Validation] (" << std::hex << messageType << ") " << pCallbackData->pMessage;
        }

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
            return create_engine(instanceExtensions,
                instanceLayers,
                deviceExtensions,
                deviceFeaturesList,
                physicalDeviceFeatures,
                validationErrors);
        }

        void shutdown()
        {
            engine.shutdown();
            instance.shutdown();
        }

    private:
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

            buffered_array<const char*, extensionsArraySize> extensions;
            buffered_array<const char*, layersArraySize> layers;

            constexpr const char* InstanceExtensions[] = {
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            };

            extensions.assign(instanceExtensions.begin(), instanceExtensions.end());
            extensions.insert(extensions.end(), std::begin(InstanceExtensions), std::end(InstanceExtensions));

            layers.assign(instanceLayers.begin(), instanceLayers.end());
            layers.emplace_back("VK_LAYER_KHRONOS_validation");

            constexpr u32 apiVersion{VK_API_VERSION_1_3};

            if (!instance.init(
                    VkApplicationInfo{
                        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                        .pNext = nullptr,
                        .pApplicationName = "vksandbox",
                        .applicationVersion = 0,
                        .pEngineName = "oblo_test_vk",
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

            constexpr const char* internalDeviceExtensions[] = {
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
                .init(instance.get(), nullptr, {}, {extensions}, &bufferDeviceAddressFeature, physicalDeviceFeatures);
        }

    public:
        instance instance;
        single_queue_engine engine;
    };
}