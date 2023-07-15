#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/render_graph/render_graph_node.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>

namespace oblo::vk
{
    struct blit_image_node
    {
        render_node_in<h32<texture>, "source"> source;
        render_node_in<h32<texture>, "destination"> destination;

        void execute(renderer_context* context)
        {
            auto& renderer = context->renderer;
            auto& resourceManager = renderer.get_resource_manager();
            auto& commandBuffer = *context->commandBuffer;

            const auto& srcTextureHandle = *source.data;
            const auto& dstTextureHandle = *destination.data;

            commandBuffer.add_pipeline_barrier(resourceManager, srcTextureHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            commandBuffer.add_pipeline_barrier(resourceManager, dstTextureHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            const auto& srcTexture = resourceManager.get(srcTextureHandle);
            const auto& dstTexture = resourceManager.get(dstTextureHandle);

            OBLO_ASSERT(srcTexture.initializer.extent.width == dstTexture.initializer.extent.width);
            OBLO_ASSERT(srcTexture.initializer.extent.height == dstTexture.initializer.extent.height);

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
                .extent = {.width = srcTexture.initializer.extent.width,
                           .height = srcTexture.initializer.extent.height,
                           .depth = 1},
            };

            vkCmdCopyImage(commandBuffer.get(),
                           srcTexture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           dstTexture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);
        }
    };
}