#include "app.hpp"

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importers/importers_module.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/systems/system_graph.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/editor/platform/init.hpp>
#include <oblo/editor/windows/asset_browser.hpp>
#include <oblo/editor/windows/inspector.hpp>
#include <oblo/editor/windows/main_window.hpp>
#include <oblo/editor/windows/scene_editing_window.hpp>
#include <oblo/editor/windows/scene_hierarchy.hpp>
#include <oblo/editor/windows/style_window.hpp>
#include <oblo/editor/windows/viewport.hpp>
#include <oblo/engine/components/name_component.hpp>
#include <oblo/engine/engine_module.hpp>
#include <oblo/engine/utility/ecs_utility.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/graphics/graphics_module.hpp>
#include <oblo/graphics/systems/static_mesh_system.hpp>
#include <oblo/graphics/systems/viewport_system.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/sandbox/context.hpp>
#include <oblo/scene/scene_module.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::editor
{
    namespace
    {
        ecs::system_seq_executor create_system_executor()
        {
            ecs::system_graph g;

            g.add_system<viewport_system>();
            g.add_system<static_mesh_system>();

            return g.instantiate();
        }
    }

    bool app::init(const vk::sandbox_init_context& ctx)
    {
        if (!platform::init())
        {
            return false;
        }

        if (!m_renderer.init({.vkContext = *ctx.vkContext, .frameAllocator = *ctx.frameAllocator}))
        {
            return false;
        }

        auto& mm = module_manager::get();
        auto* const engine = mm.load<oblo::engine_module>();
        mm.load<graphics_module>();
        mm.load<scene::scene_module>();
        mm.load<importers::importers_module>();
        auto* const reflection = mm.load<oblo::reflection::reflection_module>();

        m_entities.init(&m_typeRegistry);

        ecs_utility::register_reflected_component_types(reflection->get_registry(),
            &m_typeRegistry,
            &engine->get_property_registry());

        auto& resourceRegistry = engine->get_resource_registry();

        m_windowManager.init();

        {
            auto& globalRegistry = m_windowManager.get_global_service_registry();

            globalRegistry.add<vk::vulkan_context>().externally_owned(ctx.vkContext);
            globalRegistry.add<vk::renderer>().externally_owned(&m_renderer);
            globalRegistry.add<resource_registry>().externally_owned(&resourceRegistry);
            globalRegistry.add<asset_registry>().externally_owned(&engine->get_asset_registry());
            globalRegistry.add<property_registry>().externally_owned(&engine->get_property_registry());

            service_registry sceneRegistry{};
            sceneRegistry.add<ecs::entity_registry>().externally_owned(&m_entities);

            const auto sceneEditingWindow =
                m_windowManager.create_window<scene_editing_window>(std::move(sceneRegistry));

            m_windowManager.create_child_window<asset_browser>(sceneEditingWindow);
            m_windowManager.create_child_window<inspector>(sceneEditingWindow);
            m_windowManager.create_child_window<scene_hierarchy>(sceneEditingWindow);
            m_windowManager.create_child_window<viewport>(sceneEditingWindow);
        }

        m_services.add<vk::vulkan_context>().externally_owned(ctx.vkContext);
        m_services.add<vk::renderer>().externally_owned(&m_renderer);
        m_services.add<resource_registry>().externally_owned(&resourceRegistry);

        m_executor = create_system_executor();

        // Add a test mesh
        {
            const auto e = m_entities.create<static_mesh_component, name_component>();
            auto& meshComponent = m_entities.get<static_mesh_component>(e);
            // meshComponent.mesh = "5c2dbfb7-e2af-bce2-5a6c-c9b6ce4e9c07"_uuid;
            meshComponent.mesh = "4cab39ab-0433-cf1c-5988-b1f619227a4f"_uuid;
            // meshComponent.mesh = "529324bf-bc19-f258-2f89-bc414f859434"_uuid;
            auto& nameComponent = m_entities.get<name_component>(e);
            nameComponent.name = "Mesh";
        }

        return true;
    }

    void app::shutdown(const vk::sandbox_shutdown_context&)
    {
        m_executor.shutdown();
        m_windowManager.shutdown();
        m_renderer.shutdown();
        platform::shutdown();
    }

    void app::update(const vk::sandbox_render_context& context)
    {
        m_executor.update(ecs::system_update_context{
            .entities = &m_entities,
            .services = &m_services,
            .frameAllocator = context.frameAllocator,
        });

        auto& resourceManager = context.vkContext->get_resource_manager();
        auto& commandBuffer = context.vkContext->get_active_command_buffer();

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