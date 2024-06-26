#include <oblo/vulkan/create_render_target.hpp>

#include <oblo/vulkan/resource_manager.hpp>

namespace oblo::vk
{
    vk_result<texture> create_2d_render_target(gpu_allocator& allocator,
        u32 width,
        u32 height,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImageAspectFlags aspectMask,
        debug_label debugLabel)
    {

        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        texture res{};

        res.initializer = image_initializer{
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
            .debugLabel = debugLabel,
        };

        allocated_image allocatedImage{};
        const auto imageRes = allocator.create_image(res.initializer, &allocatedImage);

        if (imageRes != VK_SUCCESS)
        {
            return imageRes;
        }

        res.image = allocatedImage.image;
        res.allocation = allocatedImage.allocation;

        const VkImageViewCreateInfo imageViewInit{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = res.image,
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
            allocator.destroy(allocatedImage);
            return imageViewRes;
        }

        allocator.get_object_debug_utils().set_object_name(allocator.get_device(), res.view, debugLabel.get());

        return res;
    }
}
