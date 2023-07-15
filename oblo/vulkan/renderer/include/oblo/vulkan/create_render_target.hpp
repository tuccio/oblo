#pragma once

#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vk_result.hpp>

namespace oblo::vk
{
    class resource_manager;

    vk_result<texture> create_2d_render_target(allocator& allocator,
                                               u32 width,
                                               u32 height,
                                               VkFormat format,
                                               VkImageUsageFlags usage,
                                               VkImageAspectFlags aspectMask);
}