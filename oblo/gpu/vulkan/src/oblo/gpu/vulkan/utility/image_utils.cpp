#include <oblo/gpu/vulkan/utility/image_utils.hpp>

namespace oblo::vk::image_utils
{
    VkImageAspectFlags deduce_aspect_mask(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;

        case VK_FORMAT_S8_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT;

        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            // These have the stencil bit as well, but we cannot create a view for both
            // see VUID-VkDescriptorImageInfo-imageView-01976
            return VK_IMAGE_ASPECT_DEPTH_BIT;

        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    expected<VkImageView> create_image_view_2d(
        VkDevice device, VkImage image, VkFormat format, const VkAllocationCallbacks* allocationCbs)
    {
        VkImageView imageView;

        const VkImageViewCreateInfo imageViewInit{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange =
                {
                    .aspectMask = deduce_aspect_mask(format),
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        if (vkCreateImageView(device, &imageViewInit, allocationCbs, &imageView) != VK_SUCCESS)
        {
            return "Failed call to vkCreateImageView"_err;
        }

        return imageView;
    }
}