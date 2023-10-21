#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    void setup_viewport_scissor(VkCommandBuffer commandBuffer, u32 width, u32 height)
    {
        const VkViewport viewport{
            .width = f32(width),
            .height = f32(height),
            .minDepth = 0.f,
            .maxDepth = 1.f,
        };

        const VkRect2D scissor{.extent{.width = width, .height = height}};

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }
}