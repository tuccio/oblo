#pragma once

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importers/module.hpp>
#include <oblo/editor/platform/init.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/editor/windows/asset_browser.hpp>
#include <oblo/editor/windows/dock_space.hpp>
#include <oblo/editor/windows/inspector.hpp>
#include <oblo/editor/windows/main_window.hpp>
#include <oblo/editor/windows/scene_hierarchy.hpp>
#include <oblo/editor/windows/style_window.hpp>
#include <oblo/editor/windows/viewport.hpp>
#include <oblo/engine/engine_module.hpp>
#include <oblo/sandbox/context.hpp>
#include <oblo/scene/module.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>


namespace oblo::editor
{
    class app
    {
    public:
        bool init(const vk::sandbox_init_context&)
        {
            if (!platform::init())
            {
                return false;
            }

            auto& mm = module_manager::get();
            auto* const engine = mm.load<oblo::engine::engine_module>();
            mm.load<oblo::scene::module>();
            mm.load<oblo::asset::importers::module>();

            m_windowManager.create_window<dock_space>();
            m_windowManager.create_window<asset_browser>(engine->get_asset_registry());
            m_windowManager.create_window<inspector>();
            m_windowManager.create_window<scene_hierarchy>();
            m_windowManager.create_window<viewport>();
            m_windowManager.create_window<style_window>();

            return true;
        }

        void shutdown(const vk::sandbox_shutdown_context&)
        {
            platform::shutdown();
        }

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
            m_windowManager.update();
        }

    private:
        oblo::editor::window_manager m_windowManager;
    };
}