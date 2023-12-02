#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/render_pass_initializer.hpp>
#include <oblo/vulkan/graph/init_context.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    struct debug_draw_all
    {
        data<vec2u> inResolution;
        data<buffer_binding_table> inPerViewBindingTable;
        resource<texture> outRenderTarget;
        resource<texture> outDepthBuffer;

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

            builder.create(outDepthBuffer,
                {
                    .width = resolution.x,
                    .height = resolution.y,
                    .format = VK_FORMAT_D24_UNORM_S8_UINT,
                    .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                },
                resource_usage::depth_stencil_write);
        }

        void init(const init_context& context)
        {
            auto& passManager = context.get_pass_manager();

            renderPass = passManager.register_render_pass({
                .name = "Debug Draw All",
                .stages =
                    {
                        {
                            .stage = pipeline_stages::vertex,
                            .shaderSourcePath = "./vulkan/shaders/debug_draw_all/debug_draw_all.vert",
                        },
                        {
                            .stage = pipeline_stages::fragment,
                            .shaderSourcePath = "./vulkan/shaders/debug_draw_all/debug_draw_all.frag",
                        },
                    },
            });
        }

        void execute(const runtime_context& context)
        {
            const auto renderTarget = context.access(outRenderTarget);
            const auto depthBuffer = context.access(outDepthBuffer);

            auto& passManager = context.get_pass_manager();

            const auto pipeline = passManager.get_or_create_pipeline(renderPass,
                {
                    .renderTargets =
                        {
                            .colorAttachmentFormats = {renderTarget.initializer.format},
                            .depthFormat = VK_FORMAT_D24_UNORM_S8_UINT,
                        },
                    .depthStencilState =
                        {
                            .depthTestEnable = true,
                            .depthWriteEnable = true,
                            .depthCompareOp = VK_COMPARE_OP_GREATER, // We use reverse depth
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

            const VkRenderingAttachmentInfo depthAttachment{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = depthBuffer.view,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
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
                .pDepthAttachment = &depthAttachment,
            };

            setup_viewport_scissor(commandBuffer, renderWidth, renderHeight);

            if (passManager.begin_render(renderPassContext, renderInfo))
            {
                const buffer_binding_table* bindingTables[] = {
                    context.access(inPerViewBindingTable),
                };

                passManager.draw(renderPassContext,
                    context.get_resource_manager(),
                    context.get_draw_registry(),
                    bindingTables);

                passManager.end_rendering(renderPassContext);
            }
        }
    };
}