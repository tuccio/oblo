#include <oblo/gpu/vulkan/utility/pipeline_barrier.hpp>

#include <oblo/core/iterator/flags_range.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/gpu/structs.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu::vk
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
            sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
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
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        }

        default:
            sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
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
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            break;
        }

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: {
            destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;
        }

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        }

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: {
            barrier.dstAccessMask =
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        }

        case VK_IMAGE_LAYOUT_GENERAL: {
            barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
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

    namespace
    {
        VkAccessFlagBits2 deduce_memory_access(memory_access_type access)
        {
            switch (access)
            {
            case memory_access_type::any_read:
                return VK_ACCESS_2_MEMORY_READ_BIT;
            case memory_access_type::any_write:
                return VK_ACCESS_2_MEMORY_WRITE_BIT;
            default:
                unreachable();
            }
        }

        VkPipelineStageFlags2 deduce_stage_mask(pipeline_sync_stage newPass)
        {
            switch (newPass)
            {
            case pipeline_sync_stage::compute:
                return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            case pipeline_sync_stage::graphics:
                // We could actually check in which stage a texture is read using reflection and pass this info, for now
                // we'd need to add all stages we may access a texture from
                return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            case pipeline_sync_stage::transfer:
                return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            case pipeline_sync_stage::raytracing:
                return VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
            default:
                return 0;
            }
        }

        void deduce_barrier(VkImageLayout& outLayout,
            VkAccessFlags2& outAccessMask,
            VkPipelineStageFlags2& outStageMask,
            image_resource_state state,
            pipeline_sync_stage pipeline)
        {
            switch (state)
            {
            case image_resource_state::depth_stencil_read:
                outLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                outAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                outStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                break;

            case image_resource_state::depth_stencil_write:
                outLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                outAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                outStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                break;

            case image_resource_state::render_target_write:
                outLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                outAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                outStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                break;

            case image_resource_state::shader_read:
                outLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                outAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                outStageMask = deduce_stage_mask(pipeline);
                break;

            case image_resource_state::storage_read:
                outLayout = VK_IMAGE_LAYOUT_GENERAL;
                outAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                outStageMask = deduce_stage_mask(pipeline);
                break;

            case image_resource_state::storage_write:
                outLayout = VK_IMAGE_LAYOUT_GENERAL;
                outAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
                outStageMask = deduce_stage_mask(pipeline);
                break;

            case image_resource_state::transfer_source:
                OBLO_ASSERT(pipeline == pipeline_sync_stage::transfer);
                outLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                outAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                outStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                break;

            case image_resource_state::transfer_destination:
                OBLO_ASSERT(pipeline == pipeline_sync_stage::transfer);
                outLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                outAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                outStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                break;

            case image_resource_state::present:
                outLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                outStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
                break;

            default:
                OBLO_ASSERT(false);
                break;
            };
        }
    }

    void deduce_barrier(VkImageMemoryBarrier2& outBarrier, const image_state_transition& transition)
    {
        deduce_barrier(outBarrier.oldLayout,
            outBarrier.srcAccessMask,
            outBarrier.srcStageMask,
            transition.previousState,
            transition.previousPipeline);

        deduce_barrier(outBarrier.newLayout,
            outBarrier.dstAccessMask,
            outBarrier.dstStageMask,
            transition.nextState,
            transition.nextPipeline);
    }

    void deduce_barrier(VkMemoryBarrier2& outBarrier, const global_memory_barrier& memoryBarrier)
    {
        VkAccessFlags2 srcAccess{};
        VkAccessFlags2 dstAccess{};
        VkPipelineStageFlags2 srcStage{};
        VkPipelineStageFlags2 dstStage{};

        for (const auto access : flags_range{memoryBarrier.nextAccesses})
        {
            dstAccess |= deduce_memory_access(access);
        }

        for (const auto access : flags_range{memoryBarrier.previousAccesses})
        {
            srcAccess |= deduce_memory_access(access);
        }

        for (const auto pipeline : flags_range{memoryBarrier.nextPipelines})
        {
            dstStage |= deduce_stage_mask(pipeline);
        }

        for (const auto pipeline : flags_range{memoryBarrier.previousPipelines})
        {
            srcStage |= deduce_stage_mask(pipeline);
        }

        outBarrier.dstAccessMask = dstAccess;
        outBarrier.dstStageMask = dstStage;
        outBarrier.srcAccessMask = srcAccess;
        outBarrier.srcStageMask = srcStage;
    }

}