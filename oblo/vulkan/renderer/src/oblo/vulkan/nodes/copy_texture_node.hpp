#pragma once

#include <oblo/vulkan/data/copy_texture_info.hpp>
#include <oblo/vulkan/graph/frame_graph_context.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/utility/pipeline_barrier.hpp>

namespace oblo::vk
{
    struct copy_texture_node
    {
        data<copy_texture_info> inTarget;

        resource<texture> inSource;

        void build(const frame_graph_build_context& context)
        {
            context.acquire(inSource, texture_usage::transfer_source);
        }

        void execute(const frame_graph_execute_context& context)
        {
            const auto targetInfo = context.access(inTarget);

            const texture sourceTex = context.access(inSource);

            OBLO_ASSERT(targetInfo.image);
            OBLO_ASSERT(sourceTex.image);

            const VkImageCopy copy{
                .srcSubresource =
                    {
                        .aspectMask = targetInfo.aspect,
                        .layerCount = 1,
                    },
                .dstSubresource =
                    {
                        .aspectMask = targetInfo.aspect,
                        .layerCount = 1,
                    },
                .extent = sourceTex.initializer.extent,
            };

            const auto commandBuffer = context.get_command_buffer();

            add_pipeline_barrier_cmd(commandBuffer,
                targetInfo.initialLayout,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                targetInfo.image,
                sourceTex.initializer.format,
                sourceTex.initializer.arrayLayers,
                sourceTex.initializer.mipLevels);

            vkCmdCopyImage(commandBuffer,
                sourceTex.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                targetInfo.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copy);

            if (targetInfo.finalLayout != VK_IMAGE_LAYOUT_UNDEFINED)
            {
                add_pipeline_barrier_cmd(commandBuffer,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    targetInfo.finalLayout,
                    targetInfo.image,
                    sourceTex.initializer.format,
                    sourceTex.initializer.arrayLayers,
                    sourceTex.initializer.mipLevels);
            }
        }
    };
}