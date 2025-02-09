#pragma once

#include <oblo/vulkan/data/copy_texture_info.hpp>
#include <oblo/vulkan/graph/frame_graph_context.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/utility/pipeline_barrier.hpp>

namespace oblo::vk
{
    struct blit_texture
    {
        h32<transfer_pass_instance> copyPassInstance;

        resource<texture> inSource;
        resource<texture> inDestination;

        void build(const frame_graph_build_context& ctx)
        {
            copyPassInstance = ctx.transfer_pass();
            ctx.acquire(inSource, texture_usage::transfer_source);
            ctx.acquire(inDestination, texture_usage::transfer_destination);
        }

        void execute(const frame_graph_execute_context& ctx)
        {
            if (!ctx.begin_pass(copyPassInstance))
            {
                return;
            }

            const texture src = ctx.access(inSource);
            const texture dst = ctx.access(inDestination);

            OBLO_ASSERT(src.image);
            OBLO_ASSERT(dst.image);

            VkImageBlit regions[1] = {
                {
                    .srcSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .layerCount = 1,
                        },
                    .dstSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .layerCount = 1,
                        },
                },
            };

            vkCmdBlitImage(ctx.get_command_buffer(),
                src.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                dst.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                regions,
                VK_FILTER_LINEAR);

            ctx.end_pass();
        }
    };
}