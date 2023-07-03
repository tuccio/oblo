#pragma once

#include <oblo/vulkan/allocator.hpp>

namespace oblo::vk
{
    struct texture
    {
        VkImage image;
        VmaAllocation allocation;
        VkImageView view;
        image_initializer initializer;
    };
}