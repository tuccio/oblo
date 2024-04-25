#include <oblo/vulkan/nodes/bypass_culling.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>

namespace oblo::vk
{
    void bypass_culling::build(const runtime_builder& builder)
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
                    .size = u32(draw.drawCommands.bufferSize),
                },
                buffer_usage::indirect | buffer_usage::transfer_destination);

            drawBufferData[outIndex] = {
                .drawCallBuffer = drawCallBuffer,
                .sourceData = draw,
            };

            ++outIndex;
        }
    }

    void bypass_culling::execute(const runtime_context& context)
    {
        const std::span drawBufferData = *context.access(outDrawBufferData);

        const auto cb = context.get_command_buffer();

        for (const auto& draw : drawBufferData)
        {
            const auto dstBuffer = context.access(draw.drawCallBuffer);

            const VkBufferCopy region{
                .srcOffset = draw.sourceData.drawCommands.bufferOffset,
                .dstOffset = dstBuffer.offset,
                .size = draw.sourceData.drawCommands.bufferSize,
            };

            OBLO_ASSERT(draw.sourceData.drawCommands.bufferSize == dstBuffer.size);
            vkCmdCopyBuffer(cb, draw.sourceData.drawCommands.buffer, dstBuffer.buffer, 1, &region);
        }
    }
}