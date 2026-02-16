#pragma once

#include <vulkan/vulkan.h>

#include <oblo/gpu/error.hpp>

namespace oblo::gpu::vk::image_utils
{
    VkImageAspectFlags deduce_aspect_mask(VkFormat format);

    result<VkImageView> create_image_view(VkDevice device,
        VkImage image,
        VkImageViewType imageType,
        VkFormat format,
        const VkAllocationCallbacks* allocationCbs);

    inline result<VkImageView> create_image_view_2d(
        VkDevice device, VkImage image, VkFormat format, const VkAllocationCallbacks* allocationCbs)
    {
        return create_image_view(device, image, VK_IMAGE_VIEW_TYPE_2D, format, allocationCbs);
    }
}