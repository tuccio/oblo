#include <oblo/graphics/systems/viewport_system.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::graphics
{
    void viewport_system::first_update(const ecs::system_update_context& ctx)
    {
        m_vkCtx = ctx.services->find<vk::vulkan_context>();

        update(ctx);
    }

    void viewport_system::update(const ecs::system_update_context& ctx)
    {
        auto& resourceManager = m_vkCtx->get_resource_manager();
        auto& commandBuffer = m_vkCtx->get_active_command_buffer();

        for (const auto [entities, viewports] : ctx.entities->range<viewport_component>())
        {
            for (const auto& viewport : viewports)
            {
                if (!viewport.texture)
                {
                    continue;
                }

                commandBuffer.add_pipeline_barrier(m_vkCtx->get_resource_manager(),
                                                   viewport.texture,
                                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                constexpr VkClearColorValue red{
                    .float32{
                        1.f,
                        0.f,
                        0.f,
                        1.f,
                    },
                };

                const auto& texture = resourceManager.get(viewport.texture);

                const VkImageSubresourceRange range{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                };

                vkCmdClearColorImage(commandBuffer.get(),
                                     texture.image,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     &red,
                                     1,
                                     &range);

                commandBuffer.add_pipeline_barrier(resourceManager,
                                                   viewport.texture,
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }
    }
}