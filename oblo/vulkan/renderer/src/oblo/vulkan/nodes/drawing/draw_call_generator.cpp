#include <oblo/vulkan/nodes/drawing/draw_call_generator.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    namespace
    {
        struct draw_call_generator_push_constants
        {
            u32 instanceTableId;
        };
    }

    void draw_call_generator::init(const frame_graph_init_context& ctx)
    {
        drawCallGeneratorPass = ctx.register_compute_pass({
            .name = "Draw Call Generator",
            .shaderSourcePath = "./vulkan/shaders/draw_call_generator/draw_call_generator.comp",
        });

        OBLO_ASSERT(drawCallGeneratorPass);
    }

    void draw_call_generator::build(const frame_graph_build_context& ctx)
    {
        drawCallGeneratorPassInstance = ctx.compute_pass(drawCallGeneratorPass, {});

        auto& drawBufferData = ctx.access(inDrawBufferData);
        auto& drawCallBuffer = ctx.access(outDrawCallBuffer);

        const auto& drawRegistry = ctx.get_draw_registry();
        const std::span drawCalls = drawRegistry.get_draw_calls();

        if (drawCalls.empty())
        {
            drawCallBuffer = {};
            return;
        }

        drawCallBuffer = allocate_n_span<resource<buffer>>(ctx.get_frame_allocator(), drawBufferData.size());

        for (usize i = 0; i < drawBufferData.size(); ++i)
        {
            auto& draw = drawBufferData[i];

            ctx.acquire(draw.preCullingIdMap, buffer_usage::storage_read);
            ctx.acquire(draw.drawCallCountBuffer, buffer_usage::storage_read);

            drawCallBuffer[i] = ctx.create_dynamic_buffer(
                {
                    .size = u32(draw.sourceData.numInstances * sizeof(VkDrawMeshTasksIndirectCommandEXT)),
                },

                buffer_usage::storage_write);
        }

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void draw_call_generator::execute(const frame_graph_execute_context& ctx)
    {
        if (const auto pass = ctx.begin_pass(drawCallGeneratorPassInstance))
        {
            const std::span drawData = ctx.access(inDrawBufferData);
            const std::span drawCallBuffers = ctx.access(outDrawCallBuffer);

            const auto subgroupSize = ctx.get_gpu_info().subgroupSize;

            binding_table bindingTable;

            for (usize drawCallIndex = 0; drawCallIndex < drawData.size(); ++drawCallIndex)
            {
                const draw_buffer_data& currentDraw = drawData[drawCallIndex];

                bindingTable.clear();

                bindingTable.bind_buffers({
                    {"b_MeshTables"_hsv, inMeshDatabase},
                    {"b_InstanceTables"_hsv, inInstanceTables},
                    {"b_InDrawCount"_hsv, currentDraw.drawCallCountBuffer},
                    {"b_PreCullingIdMap"_hsv, currentDraw.preCullingIdMap},
                    {"b_OutDrawCallsBuffer"_hsv, drawCallBuffers[drawCallIndex]},
                });

                const u32 maxInstances = currentDraw.sourceData.numInstances;

                const draw_call_generator_push_constants pcData{
                    .instanceTableId = currentDraw.sourceData.instanceTableId,
                };

                ctx.bind_descriptor_sets(bindingTable);
                ctx.push_constants(shader_stage::compute, 0, as_bytes(std::span{&pcData, 1}));

                // We could also use the draw count to dispatch indirect here, it may be more efficient when many
                // objects are culled
                ctx.dispatch_compute(round_up_div(maxInstances, subgroupSize), 1, 1);
            }

            ctx.end_pass();
        }
    }
}