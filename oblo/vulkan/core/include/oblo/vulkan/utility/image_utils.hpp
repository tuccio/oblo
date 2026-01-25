#pragma once

#include <vulkan/vulkan.h>

#include <oblo/core/expected.hpp>

namespace oblo::vk::image_utils
{
    VkImageAspectFlags deduce_aspect_mask(VkFormat format);

    expected<VkImageView> create_image_view_2d(
        VkDevice device, VkImage image, VkFormat format, const VkAllocationCallbacks* allocationCbs);
}