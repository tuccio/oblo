#include <oblo/vulkan/nodes/frustum_culling.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/init_context.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>

namespace oblo::vk
{
    void frustum_culling::init(const init_context& context)
    {
        auto& pm = context.get_pass_manager();

        cullPass = pm.register_compute_pass({
            .name = "Frustum Culling",
            .shaderSourcePath = "./vulkan/shaders/frustum_culling/cull.comp",
        });

        OBLO_ASSERT(cullPass);
    }

    void frustum_culling::build(const runtime_builder& builder)
    {
        auto& allocator = builder.get_frame_allocator();

        auto& cullData = builder.access(outCullData);

        const auto& drawRegistry = builder.get_draw_registry();
        const std::span drawCalls = drawRegistry.get_draw_calls();

        cullData = allocate_n_span<frustum_culling_data>(allocator, drawCalls.size());

        for (usize i = 0; i < drawCalls.size(); ++i)
        {
            const auto draw = drawCalls[i];

            const auto drawCount = draw.drawCommands.drawCount;

            const auto drawBufferSize = drawCount *
                u32(draw.drawCommands.isIndexed ? sizeof(VkDrawIndexedIndirectCommand) : sizeof(VkDrawIndirectCommand));

            const auto indicesBufferSize = u32(drawCount * sizeof(u32));

            cullData[i] = {
                .drawCallBuffer = builder.create_dynamic_buffer({.size = drawBufferSize}, buffer_usage::storage),
                .preCullingIndicesBuffer =
                    builder.create_dynamic_buffer({.size = indicesBufferSize}, buffer_usage::storage),
                .sourceData = draw,
            };
        }
    }

    void frustum_culling::execute(const runtime_context& context)
    {
        auto& pm = context.get_pass_manager();

        const auto pipeline = pm.get_or_create_pipeline(cullPass, {});

        if (const auto pass = pm.begin_compute_pass(context.get_command_buffer(), pipeline))
        {
            const auto cullData = *context.access(outCullData);

            const auto subgroupSize = pm.get_subgroup_size();

            for (const auto& cullSet : cullData)
            {
                const auto count = cullSet.sourceData.drawCommands.drawCount;
                pm.dispatch(*pass, round_up_multiple(count, subgroupSize), 1, 1);
            }

            pm.end_compute_pass(*pass);
        }
    }
}
