#pragma once

#include <oblo/editor/asset_browser.hpp>
#include <oblo/editor/inspector.hpp>
#include <oblo/editor/dock_space.hpp>
#include <oblo/editor/main_window.hpp>
#include <oblo/editor/runtime.hpp>
#include <oblo/editor/style_window.hpp>
#include <oblo/sandbox/context.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <vulkan/vulkan_core.h>


namespace oblo::editor
{
    class app
    {
    public:
        bool init(const vk::sandbox_init_context&)
        {
            m_runtime.create_window<dock_space>();
            // m_runtime.create_window<main_window>();
            m_runtime.create_window<asset_browser>();
            m_runtime.create_window<inspector>();
            m_runtime.create_window<style_window>();

            return true;
        }

        void shutdown(const vk::sandbox_shutdown_context&) {}

        void update(const vk::sandbox_render_context& context)
        {
            auto& resourceManager = *context.resourceManager;
            auto& commandBuffer = *context.commandBuffer;

            commandBuffer.add_pipeline_barrier(resourceManager,
                                               context.swapchainTexture,
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            constexpr VkClearColorValue black{};

            const auto& texture = resourceManager.get(context.swapchainTexture);

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
                                 &black,
                                 1,
                                 &range);
        }

        void update_imgui(const vk::sandbox_update_imgui_context&)
        {
            m_runtime.update();
        }

    private:
        oblo::editor::runtime m_runtime;
    };
}