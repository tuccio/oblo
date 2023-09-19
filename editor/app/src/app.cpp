#include "app.hpp"

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importers/importers_module.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/systems/system_graph.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/editor/platform/init.hpp>
#include <oblo/editor/windows/asset_browser.hpp>
#include <oblo/editor/windows/dock_space.hpp>
#include <oblo/editor/windows/inspector.hpp>
#include <oblo/editor/windows/main_window.hpp>
#include <oblo/editor/windows/scene_hierarchy.hpp>
#include <oblo/editor/windows/style_window.hpp>
#include <oblo/editor/windows/viewport.hpp>
#include <oblo/engine/engine_module.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/graphics/systems/viewport_system.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/sandbox/context.hpp>
#include <oblo/scene/scene_module.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>

namespace oblo::editor
{
    namespace
    {
        ecs::system_seq_executor create_system_executor()
        {
            ecs::system_graph g;

            g.add_system<graphics::viewport_system>();

            return g.instantiate();
        }
    }

    bool app::init(const vk::sandbox_init_context& ctx)
    {
        if (!platform::init())
        {
            return false;
        }

        auto& mm = module_manager::get();
        auto* const engine = mm.load<oblo::engine::engine_module>();
        mm.load<oblo::scene::scene_module>();
        mm.load<oblo::asset::importers::importers_module>();

        {
            m_typeRegistry.register_component(ecs::make_component_type_desc<graphics::viewport_component>());
            m_entities.init(&m_typeRegistry);
        }

        m_windowManager.create_window<dock_space>();
        m_windowManager.create_window<asset_browser>(engine->get_asset_registry());
        m_windowManager.create_window<inspector>();
        m_windowManager.create_window<scene_hierarchy>();
        m_windowManager.create_window<viewport>(*ctx.allocator, *ctx.engine, *ctx.resourceManager, m_entities);
        // m_windowManager.create_window<style_window>();

        m_services.add<vk::allocator>().externally_owned(ctx.allocator);
        m_services.add<vk::resource_manager>().externally_owned(ctx.resourceManager);
        m_services.add<vk::single_queue_engine>().externally_owned(ctx.engine);
        m_services.add<vk::stateful_command_buffer*>().externally_owned(&m_currentCb);

        m_executor = create_system_executor();

        return true;
    }

    void app::shutdown(const vk::sandbox_shutdown_context&)
    {
        platform::shutdown();
    }

    void app::update(const vk::sandbox_render_context& context)
    {
        // We pass it down through the service registry because we don't have a way of manging command buffers yet
        m_currentCb = context.commandBuffer;

        m_executor.update(ecs::system_update_context{.entities = &m_entities, .services = &m_services});

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

    void app::update_imgui(const vk::sandbox_update_imgui_context&)
    {
        m_windowManager.update();
    }
}