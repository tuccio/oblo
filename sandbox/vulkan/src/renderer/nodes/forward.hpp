#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/render_graph/render_graph_node.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/render_pass_initializer.hpp>
#include <oblo/vulkan/render_pass_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/texture.hpp>
#include <renderer/create_render_target.hpp>
#include <renderer/renderer_context.hpp>
#include <renderer/textures.hpp>

namespace oblo::vk
{
    struct forward_node
    {
        // render_node_in<handle<buffer>, "camera_buffer"> cameraBuffer;
        render_node_in<handle<texture>, "render_target"> renderTarget;

        handle<render_pass> forwardPass;

        bool initialize(renderer_context* rendererContext)
        {
            const auto& context = *rendererContext->initContext;
            auto& renderPassManager = *context.renderPassManager;

            forwardPass =
                renderPassManager.register_render_pass({.name = "forward",
                                                        .stages = {
                                                            {
                                                                .stage = pipeline_stages::vertex,
                                                                .shaderSourcePath = "./shaders/forward/forward.vert",
                                                            },
                                                            {
                                                                .stage = pipeline_stages::fragment,
                                                                .shaderSourcePath = "./shaders/forward/forward.frag",
                                                            },
                                                        }});

            return bool{forwardPass};
        }

        void execute(renderer_context* rendererContext)
        {
            const auto& context = *rendererContext->renderContext;
            auto& renderPassManager = *context.renderPassManager;

            auto& commandBuffer = *context.commandBuffer;

            const VkClearColorValue clearColor{};

            const VkImageSubresourceRange imageRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            };

            commandBuffer.add_pipeline_barrier(*context.resourceManager,
                                               *renderTarget,
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            const auto& texture = context.resourceManager->get(*renderTarget);

            vkCmdClearColorImage(commandBuffer.get(),
                                 texture.image,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 &clearColor,
                                 1,
                                 &imageRange);

            const auto pipeline = renderPassManager.get_or_create_pipeline(
                *context.frameAllocator,
                forwardPass,
                {
                    .renderTargets = {.colorAttachmentFormats = {texture.initializer.format}},
                });

            if (!pipeline)
            {
                return;
            }

            const VkRenderingAttachmentInfo colorAttachmentInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = texture.view,
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            };

            const VkRenderingInfo renderInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {.extent{.width = context.width, .height = context.height}},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachmentInfo,
            };

            commandBuffer.add_pipeline_barrier(*context.resourceManager,
                                               *renderTarget,
                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            vkCmdBeginRendering(commandBuffer.get(), &renderInfo);

            renderPassManager.bind(commandBuffer.get(), pipeline);

            {
                const u32 width = context.width;
                const u32 height = context.height;

                const VkViewport viewport{
                    .width = f32(width),
                    .height = f32(height),
                    .minDepth = 0.f,
                    .maxDepth = 1.f,
                };

                const VkRect2D scissor{.extent{.width = width, .height = height}};

                vkCmdSetViewport(commandBuffer.get(), 0, 1, &viewport);
                vkCmdSetScissor(commandBuffer.get(), 0, 1, &scissor);
            }

            vkCmdEndRendering(commandBuffer.get());
        }
    };
}