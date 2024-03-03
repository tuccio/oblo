#include <oblo/vulkan/nodes/frustum_culling.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/init_context.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>

namespace oblo::vk
{
    namespace
    {
        struct frustum_culling_buffer
        {
            u32 numberOfDraws;
            u32 firstFreeIndex;
        };
    }

    void frustum_culling::init(const init_context& context)
    {
        auto& pm = context.get_pass_manager();

        cullPass = pm.register_compute_pass({
            .name = "Frustum Culling",
            .shaderSourcePath = "./vulkan/shaders/frustum_culling/cull.comp",
        });

        drawIndexedDefine = context.get_string_interner().get_or_add("DRAW_INDEXED");

        OBLO_ASSERT(cullPass);
    }

    void frustum_culling::build(const runtime_builder& builder)
    {
        auto& allocator = builder.get_frame_allocator();

        auto& cullData = builder.access(outCullData);

        const auto& drawRegistry = builder.get_draw_registry();
        const std::span drawCalls = drawRegistry.get_draw_calls();

        if (drawCalls.empty())
        {
            cullData = {};
            return;
        }

        cullData = allocate_n_span<frustum_culling_data>(allocator, drawCalls.size());

        usize first = 0;
        usize last = cullData.size();

        for (const auto& draw : drawCalls)
        {
            const u32 drawCount = draw.drawCommands.drawCount;

            // We effectively partition non-indexed and indexed calls here
            const auto outIndex = draw.drawCommands.isIndexed ? --last : first++;

            cullData[outIndex] = {
                .drawCallBuffer = builder.create_dynamic_buffer(
                    {
                        .size = u32(draw.drawCommands.bufferSize),
                    },
                    buffer_usage::storage),
                .configBuffer = builder.create_dynamic_buffer(
                    {
                        .size = sizeof(u32),
                        .data = std::as_bytes(std::span{&drawCount, 1}),
                    },
                    buffer_usage::uniform),
                .sourceData = draw,
            };
        }
    }

    void frustum_culling::execute(const runtime_context& context)
    {
        auto& pm = context.get_pass_manager();

        const h32<string> defines[] = {drawIndexedDefine};

        const std::span allDefines{defines};

        usize nextIndex = 0;

        auto& interner = context.get_string_interner();
        const auto& drawRegistry = context.get_draw_registry();

        const auto cullingConfigName = interner.get_or_add("b_CullingConfig");
        const auto inDrawCallsBufferName = interner.get_or_add("b_InDrawCallsBuffer");
        const auto outDrawCallsBufferName = interner.get_or_add("b_OutDrawCallsBuffer");

        buffer_binding_table bindingTable;

        for (const auto indexedPipeline : {false, true})
        {
            const auto pipeline =
                pm.get_or_create_pipeline(cullPass, {.defines = allDefines.subspan(0, indexedPipeline ? 1 : 0)});

            if (const auto pass = pm.begin_compute_pass(context.get_command_buffer(), pipeline))
            {
                const auto cullData = *context.access(outCullData);

                const auto subgroupSize = pm.get_subgroup_size();

                for (; nextIndex < cullData.size() &&
                     cullData[nextIndex].sourceData.drawCommands.isIndexed == indexedPipeline;
                     ++nextIndex)
                {
                    const auto& cullSet = cullData[nextIndex];

                    bindingTable.clear();

                    const buffer configBuffer = context.access(cullSet.configBuffer);
                    const buffer outDrawCallsBuffer = context.access(cullSet.drawCallBuffer);

                    bindingTable.emplace(cullingConfigName, configBuffer);

                    const auto& drawCommands = cullSet.sourceData.drawCommands;

                    bindingTable.emplace(inDrawCallsBufferName,
                        buffer{
                            .buffer = drawCommands.buffer,
                            .offset = u32(drawCommands.bufferOffset),
                            .size = u32(drawCommands.bufferSize),
                        });

                    bindingTable.emplace(outDrawCallsBufferName, outDrawCallsBuffer);

                    const u32 count = cullSet.sourceData.drawCommands.drawCount;

                    const buffer_binding_table* bindingTables[] = {
                        context.access(inPerViewBindingTable),
                        &bindingTable,
                    };

                    for (u32 i = 0; i < cullSet.sourceData.instanceBuffers.count; ++i)
                    {
                        const auto binding = cullSet.sourceData.instanceBuffers.bindings[i];
                        const auto name = drawRegistry.get_name(binding);

                        bindingTable.emplace(name, cullSet.sourceData.instanceBuffers.buffers[i]);
                    }

                    pm.dispatch(*pass, round_up_multiple(count, subgroupSize), 1, 1, bindingTables);
                }

                pm.end_compute_pass(*pass);
            }
        }
    }
}
