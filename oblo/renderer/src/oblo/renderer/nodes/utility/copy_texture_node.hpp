#pragma once

#include <oblo/renderer/data/copy_texture_info.hpp>
#include <oblo/renderer/graph/frame_graph_context.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/resource_manager.hpp>
#include <oblo/renderer/texture.hpp>
#include <oblo/renderer/utility/pipeline_barrier.hpp>

namespace oblo
{
    struct copy_texture_node
    {
        data<copy_texture_info> inTarget;
        h32<transfer_pass_instance> copyPassInstance;

        resource<texture> inSource;

        void build(const frame_graph_build_context& ctx)
        {
            copyPassInstance = ctx.transfer_pass();
            ctx.acquire(inSource, texture_usage::download);
        }

        void execute(const frame_graph_execute_context& ctx)
        {
            if (!ctx.begin_pass(copyPassInstance))
            {
                return;
            }

            const auto targetInfo = ctx.access(inTarget);

            const texture sourceTex = ctx.access(inSource);

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

            const auto commandBuffer = ctx.get_command_buffer();

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

            ctx.end_pass();
        }
    };
}