#include <oblo/scene/editor/scene_editing_window.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/ecs/systems/system_graph_usages.hpp>
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
#include <oblo/scene/assets/scene.hpp>
#include <oblo/scene/assets/traits.hpp>
#include <oblo/scene/resources/entity_hierarchy.hpp>
#include <oblo/scene/serialization/entity_hierarchy_serialization_context.hpp>
#include <oblo/scene/systems/usages.hpp>

#include <IconsFontAwesome6.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace oblo::editor
{
    enum class scene_editing_window::editor_mode : u8
    {
        editing,
        simulating,
    };

    namespace
    {
        bool create_world(runtime& world,
            const property_registry& propertyRegistry,
            const resource_registry& resourceRegistry,
            ecs::system_graph_usages usages)
        {
            auto& mm = module_manager::get();

            auto* const typeRegistry = mm.find_unique_service<const ecs::type_registry>();

            if (!typeRegistry)
            {
                return false;
            }

            if (!world.init({

                    .reflectionRegistry = &propertyRegistry.get_reflection_registry(),
                    .typeRegistry = typeRegistry,
                    .propertyRegistry = &propertyRegistry,
                    .resourceRegistry = &resourceRegistry,
                    .worldBuilders = mm.find_services<ecs::world_builder>(),
                    .usages = &usages,
                }))
            {
                return false;
            }

            return true;
        }

        ecs::system_graph_usages make_system_usages(scene_editing_window::editor_mode newMode)
        {
            using editor_mode = scene_editing_window::editor_mode;

            ecs::system_graph_usages usages;

            switch (newMode)
            {
            case editor_mode::simulating:
                usages = {system_graph_usages::editor, system_graph_usages::scripts};
                break;

            case editor_mode::editing:
                usages = {system_graph_usages::editor};
                break;

            default:
                unreachable();
            }

            return usages;
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

    scene_editing_window ::~scene_editing_window()
    {
        on_close();
    }

    bool scene_editing_window::init(const window_update_context& ctx)
    {
        m_propertyRegistry = ctx.services.find<const property_registry>();
        OBLO_ASSERT(m_propertyRegistry);

        m_resourceRegistry = ctx.services.find<const resource_registry>();
        OBLO_ASSERT(m_resourceRegistry);

        m_updateSubscriptions = ctx.services.find<update_subscriptions>();
        OBLO_ASSERT(m_updateSubscriptions);

        if (!m_propertyRegistry || !m_resourceRegistry || !m_updateSubscriptions)
        {
            return false;
        }

        m_subscription = m_updateSubscriptions->subscribe(
            [this]()
            {
                const auto now = clock::now();

                const auto dt = now - m_lastFrameTime;

                m_editorWorld.set_time_stats({.dt = dt});
                m_scene.update({.dt = dt});

                m_lastFrameTime = now;
            });

        if (!create_world(m_scene, *m_propertyRegistry, *m_resourceRegistry, make_system_usages(m_editorMode)))
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

        constexpr u32 disabledButtonColor = 0xFF545454;

        if (ImGui::BeginViewportSideBar("##scene_toolbar", nullptr, ImGuiDir_Up, height, flags))
        {
            if (ImGui::BeginMenuBar())
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 0));
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));

                if (toolbar_button(ICON_FA_FLOPPY_DISK, m_assetId.is_nil(), colors::blue, disabledButtonColor))
                {
                    auto* const assetRegistry = ctx.services.find<asset_registry>();

                    if (!assetRegistry || !save_scene(*assetRegistry))
                    {
                        log::error("Failed to save asset {}", m_assetId);
                    }
                }

                ImGui::SameLine();

                ImGui::Separator();

                ImGui::SameLine();

                if (toolbar_button(ICON_FA_PLAY,
                        m_editorMode != editor_mode::editing,
                        colors::green,
                        disabledButtonColor))
                {
                    start_simulation();
                }

                ImGui::SameLine();

                if (toolbar_button(ICON_FA_STOP,
                        m_editorMode == editor_mode::editing,
                        colors::red,
                        disabledButtonColor))
                {
                    stop_simulation();
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

    expected<> scene_editing_window::load_scene(asset_registry& assetRegistry, const uuid& assetId)
    {
        auto anyAsset = assetRegistry.load_asset(assetId);

        if (!anyAsset)
        {
            return unspecified_error;
        }

        auto* const sceneAsset = anyAsset->as<scene>();

        if (!sceneAsset)
        {
            return unspecified_error;
        }

        m_assetId = {};

        // Keeps the current editor mode
        const auto newMode = m_editorMode;

        if (!copy_current_from(*sceneAsset, newMode))
        {
            return unspecified_error;
        }

        m_assetId = assetId;
        return no_error;
    }

    expected<> scene_editing_window::save_scene(asset_registry& assetRegistry) const
    {
        any_asset asset;
        auto& sceneAsset = asset.emplace<scene>();

        if (!copy_scene_to(sceneAsset))
        {
            return unspecified_error;
        }

        return assetRegistry.save_asset(asset, m_assetId);
    }

    expected<> scene_editing_window::copy_current_from(const entity_hierarchy& source, editor_mode newMode)
    {
        m_editorWorld.detach_world();

        runtime newScene;

        // Try to create the world first, so we keep the current around in case it fails
        if (!create_world(newScene, *m_propertyRegistry, *m_resourceRegistry, make_system_usages(newMode)))
        {
            return unspecified_error;
        }

        // Now we can replace it
        m_scene.shutdown();
        m_scene = std::move(newScene);

        // This lets the viewports know they have to detach and recreate their entities
        m_editorWorld.switch_world(m_scene.get_entity_registry(), m_scene.get_service_registry());

        entity_hierarchy_serialization_context serializationCtx;

        // NOTE: Here we pass empty configs into the copy_to, thus copying transient entities and types too if present,
        // this is currently unused and possibly unnecessary, so subject to revisiting
        if (!serializationCtx.init() ||
            !source.copy_to(m_scene.get_entity_registry(), serializationCtx.get_property_registry(), {}, {}))
        {
            return unspecified_error;
        }

        return no_error;
    }

    expected<> scene_editing_window::copy_scene_to(entity_hierarchy& destination) const
    {
        const ecs::entity_registry* source{};

        switch (m_editorMode)
        {
        case editor_mode::simulating:
            source = &m_sceneBackup.get_entity_registry();
            break;

        case editor_mode::editing:
            source = &m_scene.get_entity_registry();
            break;

        default:
            return unspecified_error;
        }

        return copy_to(*source, destination);
    }

    expected<> scene_editing_window::copy_to(const ecs::entity_registry& source, entity_hierarchy& destination)
    {
        entity_hierarchy_serialization_context serializationCtx;

        // NOTE: In this copy we ignore transient entities and types (e.g. editor entities that might be present)
        if (!serializationCtx.init() || !destination.init(serializationCtx.get_type_registry()) ||
            !destination.copy_from(source,
                serializationCtx.get_property_registry(),
                serializationCtx.make_write_config(),
                serializationCtx.make_read_config()))
        {
            return unspecified_error;
        }

        return no_error;
    }

    void scene_editing_window::start_simulation()
    {
        m_sceneBackup = {};

        if (!copy_to(m_scene.get_entity_registry(), m_sceneBackup))
        {
            log::error("Failed to start simulation due to a serialization error");
            return;
        }

        if (!copy_current_from(m_sceneBackup, editor_mode::simulating))
        {
            log::error("Failed to serialize scene back");
        }

        m_editorMode = editor_mode::simulating;
    }

    void scene_editing_window::stop_simulation()
    {
        if (!copy_current_from(m_sceneBackup, editor_mode::editing))
        {
            log::error("Failed to serialize scene back");
        }

        m_editorMode = editor_mode::editing;
    }
}