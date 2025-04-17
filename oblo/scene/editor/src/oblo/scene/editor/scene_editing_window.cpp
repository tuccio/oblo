#include <oblo/scene/editor/scene_editing_window.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/update_dispatcher.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/editor/windows/command_palette_window.hpp>
#include <oblo/editor/windows/inspector.hpp>
#include <oblo/editor/windows/scene_hierarchy.hpp>
#include <oblo/editor/windows/viewport.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/runtime/runtime.hpp>
#include <oblo/scene/resources/entity_hierarchy.hpp>

#include <imgui.h>

namespace oblo::editor
{
    namespace
    {
        bool create_world(runtime& world, const service_context& ctx)
        {
            auto& mm = module_manager::get();

            auto* const reflection = mm.find<oblo::reflection::reflection_module>();

            auto* const propertyRegistry = ctx.find<const property_registry>();
            auto* const resourceRegistry = ctx.find<const resource_registry>();
            auto* const typeRegistry = mm.find_unique_service<const ecs::type_registry>();

            if (!(reflection && propertyRegistry && resourceRegistry && typeRegistry))
            {
                return false;
            }

            if (!world.init({

                    .reflectionRegistry = &reflection->get_registry(),
                    .typeRegistry = typeRegistry,
                    .propertyRegistry = propertyRegistry,
                    .resourceRegistry = resourceRegistry,
                    .worldBuilders = mm.find_services<ecs::world_builder>(),
                }))
            {
                return false;
            }

            return true;
        }
    }

    scene_editing_window ::~scene_editing_window()
    {
        on_close();
    }

    bool scene_editing_window::init(const window_update_context& ctx)
    {
        m_updateSubscriptions = ctx.services.find<update_subscriptions>();
        OBLO_ASSERT(m_updateSubscriptions);

        m_subscription = m_updateSubscriptions->subscribe(
            [this]()
            {
                const auto now = clock::now();

                const auto dt = now - m_lastFrameTime;

                m_editorWorld.set_time_stats({.dt = dt});
                m_scene.update({.dt = dt});

                m_lastFrameTime = now;
            });

        if (!create_world(m_scene, ctx.services))
        {
            return false;
        }

        m_editorWorld.switch_world(m_scene.get_entity_registry(), m_scene.get_service_registry());

        auto* const registry = ctx.services.get_local_registry();
        OBLO_ASSERT(registry);

        registry->add<editor_world>().externally_owned(&m_editorWorld);

        ctx.windowManager.create_child_window<inspector>(ctx.windowHandle);
        ctx.windowManager.create_child_window<scene_hierarchy>(ctx.windowHandle);
        ctx.windowManager.create_child_window<viewport>(ctx.windowHandle);

        m_lastFrameTime = clock::now();

        return true;
    }

    bool scene_editing_window::update(const window_update_context& ctx)
    {
        if (auto& io = ImGui::GetIO(); !io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P))
        {
            ctx.windowManager.create_child_window<command_palette_window>(ctx.windowHandle,
                window_flags::unique_sibling,
                {});
        }

        return true;
    }

    void scene_editing_window::on_close()
    {
        if (m_subscription)
        {
            m_updateSubscriptions->unsubscribe(m_subscription);
            m_subscription = {};
        }

        m_scene.shutdown();
    }

    ecs::entity_registry& scene_editing_window::get_entity_registry() const
    {
        return m_scene.get_entity_registry();
    }
}