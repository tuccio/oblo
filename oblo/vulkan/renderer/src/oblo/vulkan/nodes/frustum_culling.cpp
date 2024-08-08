#include <oblo/vulkan/nodes/frustum_culling.hpp>

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
        auto& pm = ctx.get_pass_manager();

        cullPass = pm.register_compute_pass({
            .name = "Frustum Culling",
            .shaderSourcePath = "./vulkan/shaders/frustum_culling/cull.comp",
        });

        OBLO_ASSERT(cullPass);

        ctx.set_pass_kind(pass_kind::compute);
    }

    void frustum_culling::build(const frame_graph_build_context& ctx)
    {
        auto& allocator = ctx.get_frame_allocator();

        auto& drawBufferData = ctx.access(outDrawBufferData);

        const auto& drawRegistry = ctx.get_draw_registry();
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
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        const buffer inInstanceTablesBuffer = ctx.access(inInstanceTables);

        const auto pipeline = pm.get_or_create_pipeline(cullPass, {});

        if (const auto pass = pm.begin_compute_pass(ctx.get_command_buffer(), pipeline))
        {
            const std::span drawData = ctx.access(outDrawBufferData);

            const auto subgroupSize = pm.get_subgroup_size();

            for (const auto& currentDraw : drawData)
            {
                bindingTable.clear();

                ctx.bind_buffers(bindingTable,
                    {
                        {"b_CameraBuffer", inCameraBuffer},
                        {"b_MeshTables", inMeshDatabase},
                        {"b_InstanceTables", inInstanceTables},
                        {"b_PreCullingIdMap", currentDraw.preCullingIdMap},
                        {"b_OutDrawCount", currentDraw.drawCallCountBuffer},
                    });

                const u32 count = currentDraw.sourceData.numInstances;

                const binding_table* bindingTables[] = {
                    &bindingTable,
                };

                const frustum_culling_push_constants pcData{
                    .instanceTableId = currentDraw.sourceData.instanceTableId,
                    .numberOfDraws = count,
                };

                pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&pcData, 1}));
                pm.bind_descriptor_sets(*pass, bindingTables);

                vkCmdDispatch(ctx.get_command_buffer(), round_up_div(count, subgroupSize), 1, 1);
            }

            pm.end_compute_pass(*pass);
        }
    }
}
