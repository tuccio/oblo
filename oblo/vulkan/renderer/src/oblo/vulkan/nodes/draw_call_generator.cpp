#include <oblo/vulkan/nodes/draw_call_generator.hpp>

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

        drawIndexedDefine = ctx.get_string_interner().get_or_add("DRAW_INDEXED");

        OBLO_ASSERT(drawCallGeneratorPass);
    }

    void draw_call_generator::build(const frame_graph_build_context& ctx)
    {
        auto& drawBufferData = ctx.access(inOutDrawBufferData);

        const auto& drawRegistry = ctx.get_draw_registry();
        const std::span drawCalls = drawRegistry.get_draw_calls();

        if (drawCalls.empty())
        {
            return;
        }

        for (auto& draw : drawBufferData)
        {
            constexpr u32 zero{};

            draw.drawCallCountBuffer = ctx.create_dynamic_buffer(
                {
                    .size = sizeof(u32),
                    .data = {as_bytes(std::span{&zero, 1})},
                },
                pass_kind::compute,
                buffer_usage::storage_read);

            ctx.acquire(draw.preCullingIdMap, pass_kind::compute, buffer_usage::storage_read);

            draw.drawCallBuffer = ctx.create_dynamic_buffer(
                {
                    .size = u32(draw.sourceData.drawCommands.drawCommands.size()),
                },
                pass_kind::compute,
                buffer_usage::storage_write);
        }

        acquire_instance_tables(ctx,
            inInstanceTables,
            inInstanceBuffers,
            pass_kind::compute,
            buffer_usage::storage_read);
    }

    void draw_call_generator::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        const h32<string> defines[] = {drawIndexedDefine};

        const std::span allDefines{defines};

        usize nextIndex = 0;

        auto& interner = ctx.get_string_interner();

        const auto inMeshTableName = interner.get_or_add("b_MeshTables");
        const auto inDrawCountBufferName = interner.get_or_add("b_InDrawCount");
        const auto inPreCullingIdMapBufferName = interner.get_or_add("b_PreCullingIdMap");
        const auto outDrawCallsBufferName = interner.get_or_add("b_OutDrawCallsBuffer");
        const auto inInstanceTablesName = interner.get_or_add("b_InstanceTables");

        buffer_binding_table bindingTable;

        const buffer inInstanceTablesBuffer = ctx.access(inInstanceTables);

        // Indexed and non indexed are partioned before this, in frustum_culling::build
        for (const auto indexedPipeline : {false, true})
        {
            const auto pipeline = pm.get_or_create_pipeline(drawCallGeneratorPass,
                {.defines = allDefines.subspan(0, indexedPipeline ? 1 : 0)});

            if (const auto pass = pm.begin_compute_pass(ctx.get_command_buffer(), pipeline))
            {
                const std::span drawData = ctx.access(inOutDrawBufferData);

                const auto subgroupSize = pm.get_subgroup_size();

                for (; nextIndex < drawData.size() &&
                     drawData[nextIndex].sourceData.drawCommands.isIndexed == indexedPipeline;
                     ++nextIndex)
                {
                    const auto& currentDraw = drawData[nextIndex];

                    const buffer outDrawCallsBuffer = ctx.access(currentDraw.drawCallBuffer);

                    bindingTable.clear();

                    bindingTable.emplace(inMeshTableName, ctx.access(inMeshDatabase));
                    bindingTable.emplace(inDrawCountBufferName, ctx.access(currentDraw.drawCallCountBuffer));
                    bindingTable.emplace(inPreCullingIdMapBufferName, ctx.access(currentDraw.preCullingIdMap));
                    bindingTable.emplace(outDrawCallsBufferName, outDrawCallsBuffer);
                    bindingTable.emplace(inInstanceTablesName, inInstanceTablesBuffer);

                    const u32 count = currentDraw.sourceData.drawCommands.drawCount;

                    const buffer_binding_table* bindingTables[] = {
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
}