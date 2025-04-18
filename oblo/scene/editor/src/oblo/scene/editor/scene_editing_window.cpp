#include <oblo/scene/editor/scene_editing_window.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/update_dispatcher.hpp>
#include <oblo/editor/ui/constants.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/editor/windows/command_palette_window.hpp>
#include <oblo/editor/windows/inspector.hpp>
#include <oblo/editor/windows/scene_hierarchy.hpp>
#include <oblo/editor/windows/viewport.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/runtime/runtime.hpp>
#include <oblo/scene/resources/entity_hierarchy.hpp>
#include <oblo/scene/serialization/entity_hierarchy_serialization_context.hpp>

#include <IconsFontAwesome6.h>

#include <imgui.h>
#include <imgui_internal.h>

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

        bool toolbar_button(const char* label, bool isDisabled, u32 enabledColor, u32 disabledColor)
        {
            if (isDisabled)
            {
                ImGui::BeginDisabled();
                ImGui::PushStyleColor(ImGuiCol_Text, disabledColor);
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, enabledColor);
            }

            const auto r = ImGui::Button(label);
            ImGui::PopStyleColor();

            if (isDisabled)
            {
                ImGui::EndDisabled();
            }

            return r;
        }

        bool toolbar_button(const char* label, bool isDisabled)
        {
            if (isDisabled)
            {
                ImGui::BeginDisabled();
            }

            const auto r = ImGui::Button(label);

            if (isDisabled)
            {
                ImGui::EndDisabled();
            }

            return r;
        }
    }

    enum class scene_editing_window::editor_mode : u8
    {
        editing,
        simulating,
    };

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

                // switch (m_editorMode)
                //{
                // case editor_mode::editing:
                //     m_scene.update({.dt = dt});
                //     break;

                // case editor_mode::simulating:
                //     m_simulation.update({.dt = dt});
                //     break;

                // default:
                //     unreachable();
                // }

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

        const f32 height = ImGui::GetFrameHeight();

        // Add menu bar flag and disable everything else
        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

        if (ImGui::BeginViewportSideBar("##scene_toolbar", nullptr, ImGuiDir_Up, height, flags))
        {
            if (ImGui::BeginMenuBar())
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 0));
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));

                ImGui::Button(ICON_FA_FLOPPY_DISK);

                ImGui::SameLine();

                ImGui::Separator();

                ImGui::SameLine();

                if (toolbar_button(ICON_FA_PLAY,
                        m_editorMode != editor_mode::editing,
                        colors::green,
                        colors::disabled_button))
                {
                    start_simulation(ctx);
                }

                ImGui::SameLine();

                if (toolbar_button(ICON_FA_STOP,
                        m_editorMode == editor_mode::editing,
                        colors::red,
                        colors::disabled_button))
                {
                    stop_simulation(ctx);
                }

                ImGui::SameLine();

                ImGui::Separator();

                ImGui::PopStyleColor();
                ImGui::PopStyleVar();

                ImGui::EndMenuBar();
            }
        }

        ImGui::End();

        ImGui::PopStyleVar();

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

    void scene_editing_window::start_simulation(const window_update_context&)
    {
        entity_hierarchy_serialization_context serializationCtx;

        m_sceneBackup = {};


        if (!serializationCtx.init() || !m_sceneBackup.init(serializationCtx.get_type_registry()) ||
            !m_sceneBackup.copy_from(m_scene.get_entity_registry(),
                serializationCtx.get_property_registry(),
                serializationCtx.make_write_config(),
                serializationCtx.make_read_config()))
        {
            log::error("Failed to start simulation due to a serialization error");
            return;
        }

        // m_editorWorld.switch_world(m_simulation.get_entity_registry(), m_simulation.get_service_registry());
        m_editorMode = editor_mode::simulating;
    }

    void scene_editing_window::stop_simulation(const window_update_context&)
    {
        entity_hierarchy_serialization_context serializationCtx;

        // This lets the viewports know they have to detach and recreate their entities
        m_editorWorld.switch_world(m_scene.get_entity_registry(), m_scene.get_service_registry());

        auto& sceneEntities = m_scene.get_entity_registry();
        sceneEntities.destroy_all();

        if (!serializationCtx.init() ||
            !m_sceneBackup.copy_to(m_scene.get_entity_registry(),
                serializationCtx.get_property_registry(),
                serializationCtx.make_write_config(),
                serializationCtx.make_read_config()))
        {
            log::error("Failed to serialize scene back");
        }

        m_editorMode = editor_mode::editing;
    }
}