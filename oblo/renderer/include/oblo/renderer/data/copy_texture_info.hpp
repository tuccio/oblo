#pragma once

#include <vulkan/vulkan_core.h>

namespace oblo
{
    struct copy_texture_info
    {
        VkImage image;
        VkImageLayout initialLayout;
        VkImageLayout finalLayout;
        VkImageAspectFlags aspect;
    };
}