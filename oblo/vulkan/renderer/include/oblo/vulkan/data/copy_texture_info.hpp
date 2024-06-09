#pragma once

#include <vulkan/vulkan_core.h>

namespace oblo::vk
{
    struct copy_texture_info
    {
        VkImage image;
        VkImageLayout initialLayout;
        VkImageLayout finalLayout;
        VkImageAspectFlags aspect;
    };
}