#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/render_graph/render_graph_node.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/create_render_target.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/render_pass_initializer.hpp>
#include <oblo/vulkan/render_pass_manager.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/textures.hpp>

namespace oblo::vk
{
    struct forward_node
    {
        // render_node_in<h32<buffer>, "camera_buffer"> cameraBuffer;
        render_node_in<h32<texture>, "render_target"> renderTarget;

        h32<render_pass> forwardPass;

        bool initialize(renderer_context* context)
        {
            auto& renderer = context->renderer;
            auto& renderPassManager = renderer.get_render_pass_manager();

            forwardPass = renderPassManager.register_render_pass({
                .name = "forward",
                .stages =
                    {
                        {
                            .stage = pipeline_stages::vertex,
                            .shaderSourcePath = "./vulkan/shaders/forward/forward.vert",
                        },
                        {
                            .stage = pipeline_stages::fragment,
                            .shaderSourcePath = "./vulkan/shaders/forward/forward.frag",
                        },
                    },
            });

            return bool{forwardPass};
        }

        void execute(renderer_context* context)
        {
            auto& renderer = context->renderer;
            auto& resourceManager = renderer.get_resource_manager();
            auto& renderPassManager = renderer.get_render_pass_manager();

            auto& commandBuffer = *context->commandBuffer;

            const VkClearColorValue clearColor{};

            const VkImageSubresourceRange imageRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            };

            commandBuffer.add_pipeline_barrier(resourceManager, *renderTarget, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            const auto& texture = resourceManager.get(*renderTarget);

            vkCmdClearColorImage(commandBuffer.get(),
                                 texture.image,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 &clearColor,
                                 1,
                                 &imageRange);

            const auto pipeline = renderPassManager.get_or_create_pipeline(
                context->frameAllocator,
                forwardPass,
                {
                    .renderTargets = {.colorAttachmentFormats = {texture.initializer.format}},
                });

            if (!pipeline)
            {
                return;
            }

            const auto renderWidth{texture.initializer.extent.width};
            const auto renderHeight{texture.initializer.extent.height};

            const VkRenderingAttachmentInfo colorAttachmentInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = texture.view,
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            };

            const VkRenderingInfo renderInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {.extent{.width = renderWidth, .height = renderHeight}},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachmentInfo,
            };

            commandBuffer.add_pipeline_barrier(resourceManager,
                                               *renderTarget,
                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            render_pass_context renderPassContext{
                .commandBuffer = commandBuffer.get(),
                .pipeline = pipeline,
                .frameAllocator = context->frameAllocator,
            };

            renderPassManager.begin_rendering(renderPassContext, renderInfo);

            {
                const VkViewport viewport{
                    .width = f32(renderWidth),
                    .height = f32(renderHeight),
                    .minDepth = 0.f,
                    .maxDepth = 1.f,
                };

                const VkRect2D scissor{.extent{.width = renderWidth, .height = renderHeight}};

                vkCmdSetViewport(commandBuffer.get(), 0, 1, &viewport);
                vkCmdSetScissor(commandBuffer.get(), 0, 1, &scissor);
            }

            const auto& meshTable = renderer.get_mesh_table();
            renderPassManager.bind(renderPassContext, resourceManager, meshTable);

            vkCmdDraw(commandBuffer.get(), meshTable.vertex_count(), meshTable.meshes_count(), 0, 0);

            renderPassManager.end_rendering(renderPassContext);
        }
    };
}