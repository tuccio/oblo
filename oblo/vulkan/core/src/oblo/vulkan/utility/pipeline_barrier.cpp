#include <oblo/vulkan/utility/pipeline_barrier.hpp>

#include <oblo/core/types.hpp>
#include <oblo/core/unreachable.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::vk
{
    namespace
    {
        constexpr bool is_depth_format(VkFormat format)
        {
            return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_X8_D24_UNORM_PACK32 ||
                format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_S8_UINT ||
                format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
        }

        constexpr bool has_stencil(VkFormat format)
        {
            return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
        }
    }

    void add_pipeline_barrier_cmd(VkCommandBuffer commandBuffer,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImage image,
        VkFormat format,
        VkImageSubresourceRange range)
    {
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = 0,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = range,
        };

        VkPipelineStageFlags sourceStage{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
        VkPipelineStageFlags destinationStage{0};

        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || is_depth_format(format))
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            if (has_stencil(format))
            {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        switch (oldLayout)
        {
        case VK_IMAGE_LAYOUT_UNDEFINED: {
            barrier.srcAccessMask = 0;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
        }

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        }

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        }

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
            sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        }

        default:
            break;
        }

        switch (newLayout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        }

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        }

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        }

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: {
            destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;
        }

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        }

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: {
            barrier.dstAccessMask =
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        }
        default:
            unreachable();
        }

        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void add_pipeline_barrier_cmd(VkCommandBuffer commandBuffer,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImage image,
        VkFormat format,
        u32 layerCount,
        u32 mipLevels)
    {
        add_pipeline_barrier_cmd(commandBuffer,
            oldLayout,
            newLayout,
            image,
            format,
            VkImageSubresourceRange{
                .aspectMask = 0,
                .baseMipLevel = 0,
                .levelCount = mipLevels,
                .baseArrayLayer = 0,
                .layerCount = layerCount,
            });
    }
}