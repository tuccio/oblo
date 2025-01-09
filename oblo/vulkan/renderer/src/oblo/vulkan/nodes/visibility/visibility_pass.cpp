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
    namespace
    {
        struct visibility_pass_push_constants
        {
            u32 instanceTableId;
        };
    }

    void visibility_pass::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        renderPass = passManager.register_render_pass({
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
        ctx.begin_pass(pass_kind::graphics);

        const auto resolution = ctx.access(inResolution);

        ctx.create(outVisibilityBuffer,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = VK_FORMAT_R32G32_UINT,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            },
            texture_usage::render_target_write);

        ctx.create(outDepthBuffer,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = VK_FORMAT_D24_UNORM_S8_UINT,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            },
            texture_usage::depth_stencil_write);

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
        const std::span drawData = ctx.access(inDrawData);

        const auto visibilityBuffer = ctx.access(outVisibilityBuffer);
        const auto depthBuffer = ctx.access(outDepthBuffer);

        auto& pm = ctx.get_pass_manager();

        const render_pipeline_initializer pipelineInitializer{
            .renderTargets =
                {
                    .colorAttachmentFormats = {visibilityBuffer.initializer.format},
                    .depthFormat = VK_FORMAT_D24_UNORM_S8_UINT,
                },
            .depthStencilState =
                {
                    .depthTestEnable = true,
                    .depthWriteEnable = true,
                    .depthCompareOp = VK_COMPARE_OP_GREATER, // We use reverse depthccccc
                },
            .rasterizationState =
                {
                    .polygonMode = VK_POLYGON_MODE_FILL,
                    .cullMode = VK_CULL_MODE_NONE,
                    .lineWidth = 1.f,
                },
        };

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
            .renderArea = {.extent{.width = renderWidth, .height = renderHeight}},
            .layerCount = 1,
            .colorAttachmentCount = array_size(colorAttachments),
            .pColorAttachments = colorAttachments,
            .pDepthAttachment = &depthAttachment,
        };

        const auto pipeline = pm.get_or_create_pipeline(renderPass, pipelineInitializer);

        const VkCommandBuffer commandBuffer = ctx.get_command_buffer();

        setup_viewport_scissor(commandBuffer, renderWidth, renderHeight);

        binding_table perDrawBindingTable;
        binding_table passBindingTable;

        ctx.bind_buffers(passBindingTable,
            {
                {"b_CameraBuffer", inCameraBuffer},
                {"b_MeshTables", inMeshDatabase},
                {"b_InstanceTables", inInstanceTables},
            });

        const binding_table* bindingTables[] = {
            &perDrawBindingTable,
            &passBindingTable,
        };

        if (const auto pass = pm.begin_render_pass(commandBuffer, pipeline, renderInfo))
        {
            const auto drawMeshIndirectCount = ctx.get_loaded_functions().vkCmdDrawMeshTasksIndirectCountEXT;

            const auto drawCallBufferSpan = ctx.access(inDrawCallBuffer);

            for (usize drawCallIndex = 0; drawCallIndex < drawData.size(); ++drawCallIndex)
            {
                const auto& culledDraw = drawData[drawCallIndex];

                const auto drawCallBuffer = ctx.access(drawCallBufferSpan[drawCallIndex]);
                const auto drawCallCountBuffer = ctx.access(culledDraw.drawCallCountBuffer);

                perDrawBindingTable.clear();

                ctx.bind_buffers(perDrawBindingTable,
                    {
                        {"b_PreCullingIdMap", culledDraw.preCullingIdMap},
                    });

                const visibility_pass_push_constants pushConstants{
                    .instanceTableId = culledDraw.sourceData.instanceTableId,
                };

                pm.bind_descriptor_sets(*pass, bindingTables);
                pm.push_constants(*pass, VK_SHADER_STAGE_MESH_BIT_EXT, 0, as_bytes(std::span{&pushConstants, 1}));

                drawMeshIndirectCount(commandBuffer,
                    drawCallBuffer.buffer,
                    drawCallBuffer.offset,
                    drawCallCountBuffer.buffer,
                    drawCallCountBuffer.offset,
                    culledDraw.sourceData.numInstances,
                    sizeof(VkDrawMeshTasksIndirectCommandEXT));
            }

            pm.end_render_pass(*pass);
        }
    }
}