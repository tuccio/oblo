#include <oblo/vulkan/nodes/drawing/frustum_culling.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    namespace
    {
        struct frustum_culling_push_constants
        {
            u32 instanceTableId;
            u32 numberOfDraws;
        };
    }

    struct frustum_culling_data
    {
        resource<buffer> configBuffer;
        resource<buffer> srcDrawCommands;
    };

    void frustum_culling::init(const frame_graph_init_context& ctx)
    {
        cullPass = ctx.register_compute_pass({
            .name = "Frustum Culling",
            .shaderSourcePath = "./vulkan/shaders/frustum_culling/cull.comp",
        });

        OBLO_ASSERT(cullPass);
    }

    void frustum_culling::build(const frame_graph_build_context& ctx)
    {
        cullPassInstance = ctx.compute_pass(cullPass, {});

        auto& allocator = ctx.get_frame_allocator();
        auto& drawBufferData = ctx.access(outDrawBufferData);

        const auto& drawRegistry = *ctx.access(inRenderWorld).drawRegistry;
        const std::span drawCalls = drawRegistry.get_draw_calls();

        if (drawCalls.empty())
        {
            drawBufferData = {};
            return;
        }

        drawBufferData = allocate_n_span<draw_buffer_data>(allocator, drawCalls.size());

        constexpr u32 zero{};

        for (const auto& [draw, drawBuffer] : zip_range(drawCalls, drawBufferData))
        {
            drawBuffer = {
                .drawCallCountBuffer = ctx.create_dynamic_buffer(
                    {
                        .size = sizeof(u32),
                        .data = {as_bytes(std::span{&zero, 1})},
                    },
                    buffer_usage::storage_write),
                .preCullingIdMap = ctx.create_dynamic_buffer(
                    {
                        .size = u32(draw.numInstances * sizeof(u32)),
                    },
                    buffer_usage::storage_write),
                .sourceData = draw,
            };
        }

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);
    }

    void frustum_culling::execute(const frame_graph_execute_context& ctx)
    {
        binding_table bindingTable;

        if (const auto pass = ctx.begin_pass(cullPassInstance))
        {
            const std::span drawData = ctx.access(outDrawBufferData);

            const auto subgroupSize = ctx.get_gpu_info().subgroupSize;

            for (const auto& currentDraw : drawData)
            {
                bindingTable.clear();

                bindingTable.bind_buffers({
                    {"b_CameraBuffer"_hsv, inCameraBuffer},
                    {"b_MeshTables"_hsv, inMeshDatabase},
                    {"b_InstanceTables"_hsv, inInstanceTables},
                    {"b_PreCullingIdMap"_hsv, currentDraw.preCullingIdMap},
                    {"b_OutDrawCount"_hsv, currentDraw.drawCallCountBuffer},
                });

                const u32 count = currentDraw.sourceData.numInstances;

                const frustum_culling_push_constants pcData{
                    .instanceTableId = currentDraw.sourceData.instanceTableId,
                    .numberOfDraws = count,
                };

                ctx.bind_descriptor_sets(bindingTable);
                ctx.push_constants(shader_stage::compute, 0, as_bytes(std::span{&pcData, 1}));

                ctx.dispatch_compute(round_up_div(count, subgroupSize), 1, 1);
            }

            ctx.end_pass();
        }
    }
}
