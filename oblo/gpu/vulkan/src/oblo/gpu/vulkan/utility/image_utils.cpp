#include <oblo/gpu/vulkan/utility/image_utils.hpp>

#include <oblo/gpu/error.hpp>
#include <oblo/gpu/vulkan/error.hpp>

namespace oblo::gpu::vk::image_utils
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
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    result<VkImageView> create_image_view(VkDevice device,
        VkImage image,
        VkImageViewType imageType,
        VkFormat format,
        const VkAllocationCallbacks* allocationCbs)
    {
        VkImageView imageView;

        const VkImageViewCreateInfo imageViewInit{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = imageType,
            .format = format,
            .subresourceRange =
                {
                    // We can't create a view for both depth and stencil, see VUID-VkDescriptorImageInfo-imageView-01976
                    .aspectMask = deduce_aspect_mask(format) & ~VK_IMAGE_ASPECT_STENCIL_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        const VkResult r = vkCreateImageView(device, &imageViewInit, allocationCbs, &imageView);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        return imageView;
    }
}