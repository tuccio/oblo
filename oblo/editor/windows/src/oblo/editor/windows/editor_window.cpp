#include <oblo/editor/windows/editor_window.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/asset_editor_manager.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/editor/windows/demo_window.hpp>
#include <oblo/editor/windows/frame_graph_window.hpp>
#include <oblo/editor/windows/options_editor.hpp>
#include <oblo/editor/windows/style_window.hpp>
#include <oblo/editor/windows/viewport.hpp>
#include <oblo/log/log.hpp>
#include <oblo/vulkan/events/gi_reset_event.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/renderer.hpp>

#include <imgui.h>

#include <sstream>

namespace oblo::editor
{
    namespace
    {
        uuid find_scene_asset(asset_editor_manager* assetEditorManager)
        {
            uuid sceneAssetId{};

            if (assetEditorManager)
            {
                // Either we need to adjust the dependencies, or this should be some sort of extension point
                constexpr uuid sceneAssetType = "9d257a82-a911-43c8-b8fb-1babd7117620"_uuid;
                sceneAssetId = assetEditorManager->find_unique_type_editor(sceneAssetType);
            }

            return sceneAssetId;
        }
    }

    void editor_window::init(const window_update_context& ctx)
    {
        m_assetEditorManager = ctx.services.find<asset_editor_manager>();
    }

    bool editor_window::update(const window_update_context& ctx)
    {
        constexpr ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

        auto& style = ImGui::GetStyle();
        const auto windowPadding = style.WindowPadding;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

        const ImGuiViewport* imguiViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(imguiViewport->WorkPos);
        ImGui::SetNextWindowSize(imguiViewport->WorkSize);
        ImGui::SetNextWindowViewport(imguiViewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove;

        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
        // and handle the pass-thru hole, so we ask Begin() to not render a background.
        if constexpr (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
            windowFlags |= ImGuiWindowFlags_NoBackground;

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
        // if (!opt_padding)
        // {
        // }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("DockSpace", nullptr, windowFlags);

        if (ImGui::BeginMenuBar())
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, windowPadding);

            if (ImGui::BeginMenu("File"))
            {
                const uuid sceneAssetId = find_scene_asset(m_assetEditorManager);

                constexpr auto saveScene = "Save Scene";

                if (sceneAssetId.is_nil())
                {
                    ImGui::BeginDisabled();
                    ImGui::MenuItem(saveScene);
                    ImGui::EndDisabled();
                }
                else if (ImGui::MenuItem(saveScene))
                {
                    if (!m_assetEditorManager->save_asset(ctx.windowManager, sceneAssetId))
                    {
                        log::error("Failed to save scene {}", sceneAssetId);
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Windows"))
            {
                if (ImGui::MenuItem("Viewport"))
                {
                    const uuid sceneAssetId = find_scene_asset(m_assetEditorManager);

                    if (!sceneAssetId.is_nil())
                    {
                        const auto h = m_assetEditorManager->get_window(sceneAssetId);

                        if (h)
                        {
                            ctx.windowManager.create_child_window<viewport>(h);
                        }
                    }
                }

                if (ImGui::MenuItem("Options"))
                {
                    ctx.windowManager.create_child_window<options_editor>(ctx.windowHandle);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Dev"))
            {
                if (ImGui::MenuItem("Frame Graph"))
                {
                    ctx.windowManager.create_child_window<frame_graph_window>(ctx.windowHandle,
                        window_flags::unique_sibling,
                        {});
                }

                if (ImGui::MenuItem("ImGui Demo Window"))
                {
                    ctx.windowManager.create_child_window<demo_window>(ctx.windowHandle);
                }

                if (ImGui::MenuItem("ImGui Style Window"))
                {
                    ctx.windowManager.create_child_window<style_window>(ctx.windowHandle);
                }

                auto* const renderer = ctx.services.find<vk::renderer>();

                auto& passManager = renderer->get_pass_manager();

                if (bool isEnabled = passManager.is_profiling_enabled();
                    ImGui::MenuItem("GPU profiling", nullptr, &isEnabled))
                {
                    passManager.set_profiling_enabled(isEnabled);
                }

                if (ImGui::MenuItem("Reset GI"))
                {
                    renderer->get_frame_graph().push_event(vk::gi_reset_event{});
                }

                if (ImGui::MenuItem("Copy frame graph to clipboard"))
                {
                    const auto& frameGraph = renderer->get_frame_graph();

                    std::stringstream ss;
                    frameGraph.write_dot(ss);

                    ImGui::SetClipboardText(ss.str().data());
                }

                ImGui::EndMenu();
            }

            ImGui::PopStyleVar();

            ImGui::EndMenuBar();
        }

        // if (!opt_padding)
        // {
        //     ImGui::PopStyleVar();
        // }

        // if (opt_fullscreen)
        // {
        // Pop ImGuiStyleVar_WindowPadding, ImGuiStyleVar_WindowRounding and ImGuiStyleVar_WindowBorderSize
        ImGui::PopStyleVar(3);
        // }

        // Submit the DockSpace
        ImGuiID dockspace_id = ImGui::GetID("oblo_dockspace");
        ImGui::DockSpace(dockspace_id, ImVec2{0.f, 0.f}, dockspaceFlags);

        ImGui::End();

        return true;
    }
}