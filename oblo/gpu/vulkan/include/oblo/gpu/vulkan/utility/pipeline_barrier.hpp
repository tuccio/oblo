#pragma once

#include <oblo/core/types.hpp>
#include <oblo/gpu/forward.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu::vk
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

    void deduce_barrier(VkImageMemoryBarrier2& outBarrier, const image_state_transition& transition);
    void deduce_barrier(VkMemoryBarrier2& outBarrier, const global_memory_barrier& memoryBarrier);

    VkImageLayout deduce_layout(image_resource_state state);
}