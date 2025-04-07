#include <oblo/vulkan/nodes/visibility/visibility_pass.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/draw/render_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/loaded_functions.hpp>
#include <oblo/vulkan/nodes/drawing/frustum_culling.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    void visibility_pass::init(const frame_graph_init_context& ctx)
    {
        renderPass = ctx.register_render_pass({
            .name = "Visibility Pass",
            .stages =
                {
                    {
                        .stage = pipeline_stages::mesh,
                        .shaderSourcePath = "./vulkan/shaders/visibility/visibility_pass.mesh",
                    },
                    {
                        .stage = pipeline_stages::fragment,
                        .shaderSourcePath = "./vulkan/shaders/visibility/visibility_pass.frag",
                    },
                },
        });
    }

    void visibility_pass::build(const frame_graph_build_context& ctx)
    {
        constexpr auto visibilityBufferFormat = VK_FORMAT_R32G32_UINT;

        passInstance = ctx.render_pass(renderPass,
            {
                .renderTargets =
                    {
                        .colorAttachmentFormats = {visibilityBufferFormat},
                        .depthFormat = VK_FORMAT_D24_UNORM_S8_UINT,
                        .blendStates = {{.enable = false}},
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

        const auto resolution = ctx.access(inResolution);

        ctx.create(outVisibilityBuffer,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = visibilityBufferFormat,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            },
            texture_usage::render_target_write);

        {
            // Handle the double buffering of frame buffers
            copyPassInstance = {};

            const u8 writeDepthIndex = outputIndex;
            const u8 readDepthIndex = 1 - outputIndex;

            const resource<texture> depthBuffers[] = {
                depthBuffer0,
                depthBuffer1,
            };

            ctx.create(depthBuffers[writeDepthIndex],
                {
                    .width = resolution.x,
                    .height = resolution.y,
                    .format = VK_FORMAT_D24_UNORM_S8_UINT,
                    .isStable = true,
                },
                texture_usage::depth_stencil_write);

            ctx.create(depthBuffers[readDepthIndex],
                {
                    .width = resolution.x,
                    .height = resolution.y,
                    .format = VK_FORMAT_D24_UNORM_S8_UINT,
                    .isStable = true,
                },
                texture_usage::depth_stencil_read);

            ctx.reroute(depthBuffers[writeDepthIndex], outDepthBuffer);
            ctx.reroute(depthBuffers[readDepthIndex], outLastFrameDepthBuffer);

            outputIndex = readDepthIndex;
        }

        for (const auto& drawData : ctx.access(inDrawData))
        {
            ctx.acquire(drawData.drawCallCountBuffer, buffer_usage::indirect);
            ctx.acquire(drawData.preCullingIdMap, buffer_usage::storage_read);
        }

        for (const auto& drawCallBuffer : ctx.access(inDrawCallBuffer))
        {
            ctx.acquire(drawCallBuffer, buffer_usage::indirect);
        }

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void visibility_pass::execute(const frame_graph_execute_context& ctx)
    {
        binding_table perDrawBindingTable;
        binding_table passBindingTable;

        passBindingTable.bind_buffers({
            {"b_CameraBuffer"_hsv, inCameraBuffer},
            {"b_MeshTables"_hsv, inMeshDatabase},
            {"b_InstanceTables"_hsv, inInstanceTables},
        });

        const std::span drawData = ctx.access(inDrawData);

        const auto visibilityBuffer = ctx.access(outVisibilityBuffer);
        const auto depthBuffer = ctx.access(outDepthBuffer);

        const VkRenderingAttachmentInfo colorAttachments[] = {
            {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = visibilityBuffer.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            },
        };

        const VkRenderingAttachmentInfo depthAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = depthBuffer.view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        const auto [renderWidth, renderHeight, _] = visibilityBuffer.initializer.extent;

        const VkRenderingInfo renderInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea =
                {
                    .extent{
                        .width = renderWidth,
                        .height = renderHeight,
                    },
                },
            .layerCount = 1,
            .colorAttachmentCount = array_size(colorAttachments),
            .pColorAttachments = colorAttachments,
            .pDepthAttachment = &depthAttachment,
        };

        if (!ctx.begin_pass(passInstance, renderInfo))
        {
            return;
        }

        ctx.set_viewport(renderWidth, renderHeight);
        ctx.set_scissor(0, 0, renderWidth, renderHeight);

        const binding_table* bindingTables[] = {
            &perDrawBindingTable,
            &passBindingTable,
        };

        const auto drawCallBufferSpan = ctx.access(inDrawCallBuffer);

        for (usize drawCallIndex = 0; drawCallIndex < drawData.size(); ++drawCallIndex)
        {
            const auto& culledDraw = drawData[drawCallIndex];

            perDrawBindingTable.clear();

            perDrawBindingTable.bind_buffers({
                {"b_PreCullingIdMap"_hsv, culledDraw.preCullingIdMap},
            });

            struct visibility_pass_push_constants
            {
                u32 instanceTableId;
            };

            const visibility_pass_push_constants pushConstants{
                .instanceTableId = culledDraw.sourceData.instanceTableId,
            };

            ctx.bind_descriptor_sets(bindingTables);
            ctx.push_constants(shader_stage::mesh, 0, as_bytes(std::span{&pushConstants, 1}));

            ctx.draw_mesh_tasks_indirect_count(drawCallBufferSpan[drawCallIndex],
                0,
                culledDraw.drawCallCountBuffer,
                0,
                culledDraw.sourceData.numInstances);
        }

        ctx.end_pass();
    }
}