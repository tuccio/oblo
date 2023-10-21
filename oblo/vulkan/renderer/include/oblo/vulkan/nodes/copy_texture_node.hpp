#pragma once

#include <oblo/vulkan/graph/init_context.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>

namespace oblo::vk
{
    struct copy_texture_node
    {
        data<VkBuffer> inDownloadRenderTarget;
        data<VkBuffer> inDownloadDepth;

        resource<texture> inRenderTarget;
        resource<texture> inDetphBuffer;

        void build(const runtime_builder& builder)
        {
            builder.acquire(inRenderTarget, resource_usage::transfer_source);
            builder.acquire(inDetphBuffer, resource_usage::transfer_source);
        }

        void execute(const runtime_context& context)
        {
            const auto cb = context.get_command_buffer();

            const auto srcRenderTarget = context.access(inRenderTarget);
            const auto srcDepthBuffer = context.access(inDetphBuffer);

            auto* const dstRenderTarget = context.access(inDownloadRenderTarget);
            auto* const dstDepthBuffer = context.access(inDownloadDepth);

            const VkBufferImageCopy renderTargetRegion{
                .imageSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .layerCount = 1,
                    },
                .imageExtent = srcRenderTarget.initializer.extent,
            };

            vkCmdCopyImageToBuffer(cb,
                srcRenderTarget.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                *dstRenderTarget,
                1,
                &renderTargetRegion);

            const VkBufferImageCopy depthBufferRegion{
                .imageSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                        .layerCount = 1,
                    },
                .imageExtent = srcDepthBuffer.initializer.extent,
            };

            vkCmdCopyImageToBuffer(cb,
                srcDepthBuffer.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                *dstDepthBuffer,
                1,
                &depthBufferRegion);
        }
    };
}