#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/graph/init_context.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>
#include <oblo/vulkan/render_pass_initializer.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    struct debug_triangle_node
    {
        data<vec2u> inResolution;
        resource<texture> outRenderTarget;

        h32<render_pass> renderPass{};

        void build(const runtime_builder& builder)
        {
            const auto resolution = builder.access(inResolution);

            builder.create(outRenderTarget,
                {
                    .width = resolution.x,
                    .height = resolution.y,
                    .format = VK_FORMAT_R8G8B8A8_UNORM,
                    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                },
                resource_usage::render_target_write);
        }

        void init(const init_context& context)
        {
            auto& renderPassManager = context.get_render_pass_manager();

            renderPass = renderPassManager.register_render_pass({
                .name = "Debug Triangle Node",
                .stages =
                    {
                        {
                            .stage = pipeline_stages::vertex,
                            .shaderSourcePath = "./vulkan/shaders/debug_triangle/debug_triangle.vert",
                        },
                        {
                            .stage = pipeline_stages::fragment,
                            .shaderSourcePath = "./vulkan/shaders/debug_triangle/debug_triangle.frag",
                        },
                    },
            });
        }

        void execute(const runtime_context& context)
        {
            const auto renderTarget = context.access(outRenderTarget);

            auto& renderPassManager = context.get_render_pass_manager();

            const auto pipeline = renderPassManager.get_or_create_pipeline(renderPass,
                {
                    .renderTargets =
                        {
                            .colorAttachmentFormats = {renderTarget.initializer.format},
                        },
                    .rasterizationState =
                        {
                            .polygonMode = VK_POLYGON_MODE_FILL,
                            .cullMode = VK_CULL_MODE_NONE,
                            .lineWidth = 1.f,
                        },
                });

            const VkCommandBuffer commandBuffer = context.get_command_buffer();

            render_pass_context renderPassContext{
                .commandBuffer = commandBuffer,
                .pipeline = pipeline,
            };

            const VkRenderingAttachmentInfo colorAttachment{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = renderTarget.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            };

            const auto [renderWidth, renderHeight, _] = renderTarget.initializer.extent;

            const VkRenderingInfo renderInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {.extent{.width = renderWidth, .height = renderHeight}},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachment,
            };

            renderPassManager.begin_rendering(renderPassContext, renderInfo);

            setup_viewport_scissor(commandBuffer, renderWidth, renderHeight);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            renderPassManager.end_rendering(renderPassContext);
        }
    };
}