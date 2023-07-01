#include <renderer/data/render_target.hpp>

namespace oblo::vk
{
    vk_result<render_target> create_2d_render_target(allocator& allocator,
                                                     u32 width,
                                                     u32 height,
                                                     VkFormat format,
                                                     VkImageUsageFlags usage,
                                                     VkImageAspectFlags aspectMask)
    {
        render_target res;

        const image_initializer imageInit{
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {.width = width, .height = height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .memoryUsage = memory_usage::gpu_only,
        };

        const auto imageRes = allocator.create_image(imageInit, &res.texture);

        if (imageRes != VK_SUCCESS)
        {
            return imageRes;
        }

        const VkImageViewCreateInfo imageViewInit{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = res.texture.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange =
                {
                    .aspectMask = aspectMask,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        const auto imageViewRes = vkCreateImageView(allocator.get_device(), &imageViewInit, nullptr, &res.view);

        if (imageViewRes != VK_SUCCESS)
        {
            allocator.destroy(res.texture);
            return imageViewRes;
        }

        return res;
    }
}
