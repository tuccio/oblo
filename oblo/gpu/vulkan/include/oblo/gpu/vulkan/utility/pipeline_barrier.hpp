#pragma once

#include <oblo/core/types.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo
{
    void add_pipeline_barrier_cmd(VkCommandBuffer commandBuffer,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImage image,
        VkFormat format,
        u32 layerCount,
        u32 mipLevels);

    void add_pipeline_barrier_cmd(VkCommandBuffer commandBuffer,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImage image,
        VkFormat format,
        VkImageSubresourceRange range);
}