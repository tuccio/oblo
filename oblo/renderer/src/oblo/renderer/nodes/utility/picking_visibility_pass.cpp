#include <oblo/renderer/nodes/utility/picking_visibility_pass.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/vec2i.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/data/draw_buffer_data.hpp>
#include <oblo/renderer/data/picking_configuration.hpp>
#include <oblo/renderer/draw/binding_table.hpp>
#include <oblo/renderer/draw/render_pass_initializer.hpp>
#include <oblo/renderer/graph/node_common.hpp>

#include <algorithm>

namespace oblo
{
    namespace
    {
        struct picking_visibility_pass_push_constants
        {
            u32 instanceTableId;
            u32 excludedEntityCount;
        };

        i32 clamp_i32(i32 value, i32 minValue, i32 maxValue)
        {
            return min(max(value, minValue), maxValue);
        }
    }

    void picking_visibility_pass::init(const frame_graph_init_context& ctx)
    {
        renderPass = ctx.register_render_pass({
            .name = "Picking Visibility Pass",
            .stages = make_span_initializer<render_pass_stage>({
                {
                    .stage = gpu::shader_stage::mesh,
                    .shaderSourcePath = "./vulkan/shaders/visibility/visibility_pass.mesh",
                },
                {
                    .stage = gpu::shader_stage::fragment,
                    .shaderSourcePath = "./vulkan/shaders/visibility/visibility_pass.frag",
                },
            }),
        });
    }

    void picking_visibility_pass::build(const frame_graph_build_context& ctx)
    {
        constexpr auto visibilityBufferFormat = gpu::image_format::r32g32_uint;
        constexpr hashed_string_view defines[] = {"PICKING_PASS"_hsv};

        passInstance = ctx.render_pass(renderPass,
            {
                .renderTargets =
                    {
                        .colorAttachmentFormats = make_span_initializer<gpu::image_format>({visibilityBufferFormat}),
                        .depthFormat = gpu::image_format::d24_unorm_s8_uint,
                        .blendStates = make_span_initializer<gpu::color_blend_attachment_state>({{.enable = false}}),
                    },
                .depthStencilState =
                    {
                        .depthTestEnable = true,
                        .depthWriteEnable = true,
                        .depthCompareOp = gpu::compare_op::greater, // We use reverse depth
                    },
                .rasterizationState =
                    {
                        .polygonMode = gpu::polygon_mode::fill,
                        .cullMode = {},
                        .lineWidth = 1.f,
                    },
                .defines = defines,
            });

        const auto resolution = ctx.access(inResolution);

        ctx.create(outVisibilityBuffer,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = visibilityBufferFormat,
            },
            texture_usage::render_target_write);

        ctx.create(outDepthBuffer,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = gpu::image_format::d24_unorm_s8_uint,
            },
            texture_usage::depth_stencil_write);

        const auto& pickingConfig = ctx.access(inPickingConfiguration);

        const u32 excludedEntityCount = min(pickingConfig.excludedEntityCount, MaxPickingExcludedEntities);

        // Always upload at least one element so the buffer is never empty, even when nothing is excluded.
        const u32 bufferElementCount = max(1u, excludedEntityCount);
        auto* const bufferData = allocate_n<u32>(ctx.get_frame_allocator(), bufferElementCount);
        std::copy_n(pickingConfig.excludedEntityIds, excludedEntityCount, bufferData);
        bufferData[bufferElementCount - 1] = 0;

        excludedEntities = ctx.create_dynamic_buffer(
            {
                .size = bufferElementCount * sizeof(u32),
                .data = as_bytes(std::span{bufferData, bufferElementCount}),
            },
            buffer_usage::storage_read);

        ctx.acquire(excludedEntities, buffer_usage::storage_read);

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
        ctx.acquire(inEntitySetBuffer, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void picking_visibility_pass::execute(const frame_graph_execute_context& ctx)
    {
        binding_table perDrawBindingTable;
        binding_table passBindingTable;

        passBindingTable.bind_buffers({
            {"b_CameraBuffer"_hsv, inCameraBuffer},
            {"b_MeshTables"_hsv, inMeshDatabase},
            {"b_InstanceTables"_hsv, inInstanceTables},
            {"b_EcsEntitySet"_hsv, inEntitySetBuffer},
            {"b_ExcludedEntities"_hsv, excludedEntities},
        });

        const auto resolution = ctx.get_resolution(outVisibilityBuffer);

        const auto& pickingConfig = ctx.access(inPickingConfiguration);

        const i32 resX = i32(resolution.x);
        const i32 resY = i32(resolution.y);

        const i32 regionSizeX = i32(min(PickingRegionSize, resolution.x));
        const i32 regionSizeY = i32(min(PickingRegionSize, resolution.y));

        const i32 cursorX = i32(pickingConfig.coordinates.x + .5f);
        const i32 cursorY = i32(pickingConfig.coordinates.y + .5f);

        const vec2i regionOffset{
            clamp_i32(cursorX - (regionSizeX - 1) / 2, 0, resX - regionSizeX),
            clamp_i32(cursorY - (regionSizeY - 1) / 2, 0, resY - regionSizeY),
        };

        const gpu::graphics_attachment colorAttachments[] = {
            {
                .image = ctx.access(outVisibilityBuffer),
                .loadOp = gpu::attachment_load_op::clear,
                .storeOp = gpu::attachment_store_op::store,
            },
        };

        const gpu::graphics_attachment depthAttachment{
            .image = ctx.access(outDepthBuffer),
            .loadOp = gpu::attachment_load_op::clear,
            .storeOp = gpu::attachment_store_op::store,
        };

        const gpu::graphics_pass_descriptor cfg{
            .renderOffset = regionOffset,
            .renderResolution = {u32(regionSizeX), u32(regionSizeY)},
            .colorAttachments = colorAttachments,
            .depthAttachment = depthAttachment,
        };

        if (!ctx.begin_pass(passInstance, cfg))
        {
            return;
        }

        ctx.set_viewport(u32(regionSizeX), u32(regionSizeY));
        ctx.set_scissor(regionOffset.x, regionOffset.y, u32(regionSizeX), u32(regionSizeY));

        const binding_table* bindingTables[] = {
            &perDrawBindingTable,
            &passBindingTable,
        };

        const std::span drawData = ctx.access(inDrawData);
        const auto drawCallBufferSpan = ctx.access(inDrawCallBuffer);

        const u32 excludedEntityCount = min(pickingConfig.excludedEntityCount, MaxPickingExcludedEntities);

        for (usize drawCallIndex = 0; drawCallIndex < drawData.size(); ++drawCallIndex)
        {
            const draw_buffer_data& culledDraw = drawData[drawCallIndex];
            OBLO_ASSERT(culledDraw.sourceData.kind == batch_kind::draw);

            perDrawBindingTable.clear();

            perDrawBindingTable.bind_buffers({
                {"b_PreCullingIdMap"_hsv, culledDraw.preCullingIdMap},
            });

            const picking_visibility_pass_push_constants pushConstants{
                .instanceTableId = culledDraw.sourceData.instanceTableId,
                .excludedEntityCount = excludedEntityCount,
            };

            ctx.bind_descriptor_sets(bindingTables);
            ctx.push_constants(gpu::shader_stage::mesh, 0, as_bytes(std::span{&pushConstants, 1}));

            ctx.draw_mesh_tasks_indirect_count(drawCallBufferSpan[drawCallIndex],
                0,
                culledDraw.drawCallCountBuffer,
                0,
                culledDraw.sourceData.numInstances);
        }

        ctx.end_pass();
    }
}
