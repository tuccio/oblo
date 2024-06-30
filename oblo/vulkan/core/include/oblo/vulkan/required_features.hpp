#pragma once

#include <span>

#include <vulkan/vulkan_core.h>

namespace oblo::vk
{
    struct required_features
    {
        std::span<const char* const> instanceExtensions;
        std::span<const char* const> deviceExtensions;
        VkPhysicalDeviceFeatures2 physicalDeviceFeatures;
        void* deviceFeaturesChain;
    };
}