#pragma once

#include <oblo/vulkan/gpu_temporary_aliases.hpp>

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