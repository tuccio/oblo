#include <oblo/vulkan/nodes/draw_call_generator.hpp>

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
        auto& pm = ctx.get_pass_manager();

        drawCallGeneratorPass = pm.register_compute_pass({
            .name = "Draw Call Generator",
            .shaderSourcePath = "./vulkan/shaders/draw_call_generator/draw_call_generator.comp",
        });

        OBLO_ASSERT(drawCallGeneratorPass);

        ctx.set_pass_kind(pass_kind::compute);
    }

    void draw_call_generator::build(const frame_graph_build_context& ctx)
    {
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

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void draw_call_generator::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        usize nextIndex = 0;

        auto& interner = ctx.get_string_interner();

        const auto inMeshTableName = interner.get_or_add("b_MeshTables");
        const auto inDrawCountBufferName = interner.get_or_add("b_InDrawCount");
        const auto inPreCullingIdMapBufferName = interner.get_or_add("b_PreCullingIdMap");
        const auto outDrawCallsBufferName = interner.get_or_add("b_OutDrawCallsBuffer");
        const auto inInstanceTablesName = interner.get_or_add("b_InstanceTables");

        binding_table bindingTable;

        const buffer inInstanceTablesBuffer = ctx.access(inInstanceTables);

        const auto pipeline = pm.get_or_create_pipeline(drawCallGeneratorPass, {});

        if (const auto pass = pm.begin_compute_pass(ctx.get_command_buffer(), pipeline))
        {
            const std::span drawData = ctx.access(inDrawBufferData);
            const std::span drawCallBuffers = ctx.access(outDrawCallBuffer);

            const auto subgroupSize = pm.get_subgroup_size();

            for (const auto& currentDraw : drawData)
            {
                const buffer outDrawCallsBuffer = ctx.access(drawCallBuffers[nextIndex]);

                bindingTable.clear();

                bindingTable.emplace(inMeshTableName, make_bindable_object(ctx.access(inMeshDatabase)));
                bindingTable.emplace(inDrawCountBufferName,
                    make_bindable_object(ctx.access(currentDraw.drawCallCountBuffer)));
                bindingTable.emplace(inPreCullingIdMapBufferName,
                    make_bindable_object(ctx.access(currentDraw.preCullingIdMap)));
                bindingTable.emplace(outDrawCallsBufferName, make_bindable_object(outDrawCallsBuffer));
                bindingTable.emplace(inInstanceTablesName, make_bindable_object(inInstanceTablesBuffer));

                const u32 count = currentDraw.sourceData.numInstances;

                const binding_table* bindingTables[] = {
                    &bindingTable,
                };

                const draw_call_generator_push_constants pcData{
                    .instanceTableId = currentDraw.sourceData.instanceTableId};

                pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&pcData, 1}));
                pm.bind_descriptor_sets(*pass, bindingTables);

                // We could also use the draw count to dispatch indirect here, it may be more efficient when many
                // objects are culled
                vkCmdDispatch(ctx.get_command_buffer(), round_up_multiple(count, subgroupSize), 1, 1);
            }

            pm.end_compute_pass(*pass);
        }
    }
}