#include <oblo/vulkan/nodes/surfels/surfel_debug.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/render_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    void surfel_debug::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        debugPass = passManager.register_render_pass({
            .name = "GI Debug Pass",
            .stages =
                {
                    {
                        .stage = pipeline_stages::vertex,
                        .shaderSourcePath = "./vulkan/shaders/surfels/surfel_debug_view.vert",
                    },
                    {
                        .stage = pipeline_stages::fragment,
                        .shaderSourcePath = "./vulkan/shaders/surfels/surfel_debug_view.frag",
                    },
                },
        });
    }

    void surfel_debug::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::graphics);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);

        ctx.acquire(inOutImage, texture_usage::render_target_write);
        ctx.acquire(inDepthBuffer, texture_usage::depth_stencil_read);

        ctx.acquire(inSurfelsPool, buffer_usage::storage_read);
    }

    void surfel_debug::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        const auto image = ctx.access(inOutImage);
        const auto depth = ctx.access(inDepthBuffer);

        const render_pipeline_initializer pipelineInitializer{
            .renderTargets =
                {
                    .colorAttachmentFormats = {image.initializer.format},
                    .depthFormat = depth.initializer.format,
                },
            .depthStencilState =
                {
                    .depthTestEnable = true,
                    .depthWriteEnable = false,
                    .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
                },
            .rasterizationState =
                {
                    .polygonMode = VK_POLYGON_MODE_FILL,
                    .cullMode = VK_CULL_MODE_NONE,
                    .lineWidth = 1.f,
                },
            .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
        };

        const VkRenderingAttachmentInfo colorAttachments[] = {
            {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = image.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            },
        };

        const VkRenderingAttachmentInfo depthAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = depth.view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_NONE,
        };

        const auto [renderWidth, renderHeight, _] = image.initializer.extent;

        const VkRenderingInfo renderInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea =
                {
                    .extent =
                        {
                            .width = renderWidth,
                            .height = renderHeight,
                        },
                },
            .layerCount = 1,
            .colorAttachmentCount = array_size(colorAttachments),
            .pColorAttachments = colorAttachments,
            .pDepthAttachment = &depthAttachment,
        };

        const auto pipeline = pm.get_or_create_pipeline(debugPass, pipelineInitializer);

        const VkCommandBuffer commandBuffer = ctx.get_command_buffer();

        setup_viewport_scissor(commandBuffer, renderWidth, renderHeight);

        if (const auto pass = pm.begin_render_pass(commandBuffer, pipeline, renderInfo))
        {
            binding_table bindingTable;

            ctx.bind_buffers(bindingTable,
                {
                    {"b_CameraBuffer", inCameraBuffer},
                    {"b_SurfelsPool", inSurfelsPool},
                });

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            vkCmdDraw(commandBuffer, 4, 1 << 16u, 0, 0);

            pm.end_render_pass(*pass);
        }
    }
}