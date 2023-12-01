#include <oblo/vulkan/nodes/picking_readback.hpp>

#include <oblo/vulkan/graph/render_graph.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>

namespace oblo::vk
{
    void picking_readback::init(const init_context&) {}

    void picking_readback::build(const runtime_builder& context)
    {
        isPickingEnabled = context.access(inPickingConfiguration).enabled;

        if (isPickingEnabled)
        {
            context.acquire(inPickingIdBuffer, resource_usage::transfer_source);
        }
    }

    void picking_readback::execute(const runtime_context& context)
    {
        // TODO: Disable execution from build instead
        if (!isPickingEnabled)
        {
            return;
        }

        const auto* cfg = context.access(inPickingConfiguration);
        const auto pickingBuffer = context.access(inPickingIdBuffer);

        const VkBufferImageCopy copyRegion{
            .bufferOffset = cfg->downloadBuffer.offset,
            .bufferRowLength = pickingBuffer.initializer.extent.width,
            .bufferImageHeight = pickingBuffer.initializer.extent.height,
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .imageOffset = {i32(cfg->coordinates.x), i32(cfg->coordinates.y), 0},
            .imageExtent = {1, 1, 1},
        };

        vkCmdCopyImageToBuffer(context.get_command_buffer(),
            pickingBuffer.image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            cfg->downloadBuffer.buffer,
            1,
            &copyRegion);
    }
}
