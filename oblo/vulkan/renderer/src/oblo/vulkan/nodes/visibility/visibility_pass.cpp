#include <oblo/vulkan/nodes/visibility/visibility_pass.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/draw/render_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/graph/render_pass.hpp>
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
        constexpr auto visibilityBufferFormat = texture_format::r32g32_uint;

        passInstance = ctx.render_pass(renderPass,
            {
                .renderTargets =
                    {
                        .colorAttachmentFormats = {visibilityBufferFormat},
                        .depthFormat = texture_format::d24_unorm_s8_uint,
                        .blendStates = {{.enable = false}},
                    },
                .depthStencilState =
                    {
                        .depthTestEnable = true,
                        .depthWriteEnable = true,
                        .depthCompareOp = compare_op::greater, // We use reverse depth
                    },
                .rasterizationState =
                    {
                        .polygonMode = polygon_mode::fill,
                        .cullMode = {},
                        .lineWidth = 1.f,
                    },
            });

        const auto resolution = ctx.access(inResolution);

        ctx.create(outVisibilityBuffer,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = visibilityBufferFormat,
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
                    .format = texture_format::d24_unorm_s8_uint,
                    .isStable = true,
                },
                texture_usage::depth_stencil_write);

            ctx.create(depthBuffers[readDepthIndex],
                {
                    .width = resolution.x,
                    .height = resolution.y,
                    .format = texture_format::d24_unorm_s8_uint,
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

        const render_attachment colorAttachments[] = {
            {
                .texture = outVisibilityBuffer,
                .loadOp = attachment_load_op::clear,
                .storeOp = attachment_store_op::store,
            },
        };

        const render_attachment depthAttachment{
            .texture = outDepthBuffer,
            .loadOp = attachment_load_op::clear,
            .storeOp = attachment_store_op::store,
        };

        const render_pass_config cfg{
            .renderResolution = ctx.get_resolution(outVisibilityBuffer),
            .colorAttachments = colorAttachments,
            .depthAttachment = depthAttachment,
        };

        if (!ctx.begin_pass(passInstance, cfg))
        {
            return;
        }

        ctx.set_viewport(cfg.renderResolution.x, cfg.renderResolution.y);
        ctx.set_scissor(0, 0, cfg.renderResolution.x, cfg.renderResolution.y);

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