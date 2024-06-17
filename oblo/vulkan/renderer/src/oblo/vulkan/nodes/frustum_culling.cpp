#include <oblo/vulkan/nodes/frustum_culling.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    namespace
    {
        struct frustum_culling_config
        {
            u32 numberOfDraws;
        };
    }

    struct frustum_culling_data
    {
        resource<buffer> configBuffer;
        resource<buffer> srcDrawCommands;
    };

    void frustum_culling::init(const frame_graph_init_context& context)
    {
        auto& pm = context.get_pass_manager();

        cullPass = pm.register_compute_pass({
            .name = "Frustum Culling",
            .shaderSourcePath = "./vulkan/shaders/frustum_culling/cull.comp",
        });

        drawIndexedDefine = context.get_string_interner().get_or_add("DRAW_INDEXED");

        OBLO_ASSERT(cullPass);
    }

    void frustum_culling::build(const frame_graph_build_context& builder)
    {
        auto& allocator = builder.get_frame_allocator();

        auto& cullData = builder.access(cullInternalData);
        auto& drawBufferData = builder.access(outDrawBufferData);

        const auto& drawRegistry = builder.get_draw_registry();
        const std::span drawCalls = drawRegistry.get_draw_calls();

        if (drawCalls.empty())
        {
            cullData = {};
            drawBufferData = {};
            return;
        }

        cullData = allocate_n_span<frustum_culling_data>(allocator, drawCalls.size());
        drawBufferData = allocate_n_span<draw_buffer_data>(allocator, drawCalls.size());

        usize first = 0;
        usize last = cullData.size();

        for (const auto& draw : drawCalls)
        {
            const frustum_culling_config config{
                .numberOfDraws = draw.drawCommands.drawCount,
            };

            const std::span instanceBuffers = allocate_n_span<resource<buffer>>(allocator, draw.instanceBuffers.count);

            // We effectively partition non-indexed and indexed calls here
            const auto outIndex = draw.drawCommands.isIndexed ? --last : first++;

            cullData[outIndex] = {
                .configBuffer = builder.create_dynamic_buffer(
                    {
                        .size = sizeof(frustum_culling_config),
                        .data = std::as_bytes(std::span{&config, 1}),
                    },
                    pass_kind::compute,
                    buffer_usage::uniform),
                .srcDrawCommands = builder.create_dynamic_buffer(
                    {
                        .size = u32(draw.drawCommands.drawCommands.size()),
                        .data = draw.drawCommands.drawCommands,
                    },
                    pass_kind::compute,
                    buffer_usage::storage_write),
            };

            drawBufferData[outIndex] = {
                .drawCallBuffer = builder.create_dynamic_buffer(
                    {
                        .size = u32(draw.drawCommands.drawCommands.size()),
                    },
                    pass_kind::compute,
                    buffer_usage::storage_write),
                .sourceData = draw,
                .instanceBuffers = instanceBuffers.data(),
            };

            for (u32 bufferIndex = 0; bufferIndex < draw.instanceBuffers.count; ++bufferIndex)
            {
                instanceBuffers[bufferIndex] =
                    builder.create_dynamic_buffer(draw.instanceBuffers.buffersData[bufferIndex],
                        pass_kind::compute,
                        buffer_usage::storage_read);
            }
        }
    }

    void frustum_culling::execute(const frame_graph_execute_context& context)
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
                const std::span cullData = context.access(cullInternalData);
                const std::span drawData = context.access(outDrawBufferData);

                const auto subgroupSize = pm.get_subgroup_size();

                for (; nextIndex < cullData.size() &&
                     drawData[nextIndex].sourceData.drawCommands.isIndexed == indexedPipeline;
                     ++nextIndex)
                {
                    const auto& currentDraw = drawData[nextIndex];
                    const auto& internalData = cullData[nextIndex];

                    bindingTable.clear();

                    const buffer configBuffer = context.access(internalData.configBuffer);
                    const buffer outDrawCallsBuffer = context.access(currentDraw.drawCallBuffer);

                    bindingTable.emplace(cullingConfigName, configBuffer);

                    const auto srcBuffer = context.access(internalData.srcDrawCommands);

                    bindingTable.emplace(inDrawCallsBufferName,
                        buffer{
                            .buffer = srcBuffer.buffer,
                            .offset = u32(srcBuffer.offset),
                            .size = u32(srcBuffer.size),
                        });

                    bindingTable.emplace(outDrawCallsBufferName, outDrawCallsBuffer);

                    const u32 count = currentDraw.sourceData.drawCommands.drawCount;

                    const buffer_binding_table* bindingTables[] = {
                        &context.access(inPerViewBindingTable),
                        &bindingTable,
                    };

                    for (u32 i = 0; i < currentDraw.sourceData.instanceBuffers.count; ++i)
                    {
                        const auto binding = currentDraw.sourceData.instanceBuffers.bindings[i];
                        const auto name = drawRegistry.get_name(binding);

                        const buffer instanceBuffer = context.access(currentDraw.instanceBuffers[i]);
                        OBLO_ASSERT(instanceBuffer.buffer);

                        bindingTable.emplace(name, instanceBuffer);
                    }

                    pm.dispatch(*pass, round_up_multiple(count, subgroupSize), 1, 1, bindingTables);
                }

                pm.end_compute_pass(*pass);
            }
        }
    }
}
