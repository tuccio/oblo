#pragma once

#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vk_result.hpp>

namespace oblo::vk
{
    struct render_target
    {
        texture texture;
        VkImageView view;
    };

    vk_result<render_target> create_2d_render_target(allocator& allocator,
                                                     u32 width,
                                                     u32 height,
                                                     VkFormat format,
                                                     VkImageUsageFlags usage,
                                                     VkImageAspectFlags aspectMask);
}