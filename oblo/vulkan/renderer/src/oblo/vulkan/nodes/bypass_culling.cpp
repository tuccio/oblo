#include <oblo/vulkan/nodes/bypass_culling.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    void bypass_culling::build(const frame_graph_build_context& builder)
    {
        auto& drawBufferData = builder.access(outDrawBufferData);

        const auto& drawRegistry = builder.get_draw_registry();
        const std::span drawCalls = drawRegistry.get_draw_calls();

        if (drawCalls.empty())
        {
            drawBufferData = {};
            return;
        }

        auto& allocator = builder.get_frame_allocator();

        drawBufferData = allocate_n_span<draw_buffer_data>(allocator, drawCalls.size());

        u32 outIndex{};

        for (const auto& draw : drawCalls)
        {
            const auto drawCallBuffer = builder.create_dynamic_buffer(
                {
                    .size = u32(draw.drawCommands.drawCommands.size()),
                    .data = draw.drawCommands.drawCommands,
                },
                pass_kind::graphics,
                buffer_usage::indirect); // TODO: This is actually not used

            drawBufferData[outIndex] = {
                .drawCallBuffer = drawCallBuffer,
                .sourceData = draw,
            };

            ++outIndex;
        }
    }
}