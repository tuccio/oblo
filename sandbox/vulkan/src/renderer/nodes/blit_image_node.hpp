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
        render_node_in<handle<texture>, "source"> source;
        render_node_in<handle<texture>, "destination"> destination;

        void execute(renderer_context* rendererContext)
        {
            const auto& context = *rendererContext->renderContext;

            const auto& srcTextureHandle = *source.data;
            const auto& dstTextureHandle = *destination.data;

            context.commandBuffer->add_pipeline_barrier(*context.resourceManager,
                                                        srcTextureHandle,
                                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            context.commandBuffer->add_pipeline_barrier(*context.resourceManager,
                                                        dstTextureHandle,
                                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            const auto& srcTexture = context.resourceManager->get(srcTextureHandle);
            const auto& dstTexture = context.resourceManager->get(dstTextureHandle);

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

            vkCmdCopyImage(context.commandBuffer->get(),
                           srcTexture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           dstTexture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);
        }
    };
}