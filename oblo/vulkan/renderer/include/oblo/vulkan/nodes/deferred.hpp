#if 0

#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/render_graph/render_graph_node.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/create_render_target.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/textures.hpp>

namespace oblo::vk
{
    struct gbuffer
    {
        texture buffers[2];
    };

    struct deferred_gbuffer_node
    {
        render_node_in<allocated_buffer, "camera"> camera;
        // render_node_out<gbuffer, "gbuffer"> gbuffer;
        // render_node_out<render_target, "depth"> depth;
        render_node_out<h32<texture>, "test"> test;

        void execute(renderer_context* rendererContext)
        {
            const auto& context = *rendererContext->renderContext;
            const auto& state = rendererContext->state;

            if (state.lastFrameHeight != context.height || state.lastFrameWidth != context.width)
            {
                // TODO: Deferred delete previous textures
                auto& allocator = *context.allocator;

                // if (allocator->create_image({}, depth.data) != VK_SUCCESS)
                // {
                //     return;
                // }

                auto rt = create_2d_render_target(allocator,
                                                  context.width,
                                                  context.height,
                                                  VK_FORMAT_B8G8R8A8_UNORM,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                  VK_IMAGE_ASPECT_COLOR_BIT);

                OBLO_ASSERT(rt);

                *test.data = context.resourceManager->register_texture(*rt, VK_IMAGE_LAYOUT_UNDEFINED);
            }

            const VkClearColorValue clearColor{.float32 = {1.f, 1.f, 0.f, 0.f}};

            const VkImageSubresourceRange imageRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            };

            context.commandBuffer->add_pipeline_barrier(*context.resourceManager,
                                                        *test.data,
                                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            const auto& texture = context.resourceManager->get(*test);

            vkCmdClearColorImage(context.commandBuffer->get(),
                                 texture.image,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 &clearColor,
                                 1,
                                 &imageRange);
        }

        void shutdown(renderer_context* rendererContext)
        {
            const auto& context = *rendererContext->shutdownContext;

            try_destroy_texture(context, *test);
        }
    };

    struct deferred_lighting_node
    {
        render_node_in<allocated_buffer, "camera"> camera;
        // render_node_in<gbuffer, "gbuffer"> gbuffer;
        // render_node_out<allocated_image, "lit"> lit;

        render_node_in<h32<texture>, "test"> test;

        void execute(renderer_context* /*rendererContext*/)
        {
            /*const auto& context = *rendererContext->renderContext;

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
                .extent = {.width = context.width, .height = context.height, .depth = 1},
            };

            vkCmdCopyImage(context.commandBuffer->get(),
                           test.data->texture.image,
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           context.swapchainImage,
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           1,
                           &region);*/
        }
    };
}

#endif