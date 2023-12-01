#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/pass_manager.hpp>
#include <oblo/vulkan/render_pass_initializer.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>


namespace oblo::vk
{
    struct clear_render_target
    {
        h32<texture> renderTarget;

        void execute(renderer_context* context)
        {
            auto& renderer = context->renderer;
            auto& resourceManager = renderer.get_resource_manager();
            auto& commandBuffer = renderer.get_active_command_buffer();

            constexpr VkClearColorValue clearColor{};

            if (renderTarget)
            {
                const auto& texture = resourceManager.get(renderTarget);

                constexpr VkImageSubresourceRange imageRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1,
                };

                vkCmdClearColorImage(commandBuffer.get(),
                    texture.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    &clearColor,
                    1,
                    &imageRange);
            }
        }
    };
}