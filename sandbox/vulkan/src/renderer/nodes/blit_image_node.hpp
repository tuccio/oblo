#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/render_graph/render_graph_node.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <renderer/renderer_context.hpp>
#include <sandbox/context.hpp>

namespace oblo::vk
{
    struct blit_image_node
    {
        render_node_in<texture, "source"> source;
        render_node_in<texture, "destination"> destination;

        void execute(renderer_context* rendererContext)
        {
            const auto& context = *rendererContext->renderContext;

            const auto& srcTexture = *source.data;
            const auto& dstTexture = *destination.data;

            context.commandBuffer->add_pipeline_barrier(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcTexture);
            context.commandBuffer->add_pipeline_barrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dstTexture);

            OBLO_ASSERT(srcTexture.extent.width == dstTexture.extent.width);
            OBLO_ASSERT(srcTexture.extent.height == dstTexture.extent.height);

            const VkImageCopy region{
                .srcSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .dstSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .extent = {.width = srcTexture.extent.width, .height = srcTexture.extent.height, .depth = 1},
            };

            vkCmdCopyImage(context.commandBuffer->get(),
                           source.data->image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           destination.data->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);
        }
    };
}