#include <oblo/editor/windows/scene_hierarchy.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/engine/components/name_component.hpp>

#include <imgui.h>

namespace oblo::editor
{
    void scene_hierarchy::init(const window_update_context& ctx)
    {
        m_registry = ctx.services.find<ecs::entity_registry>();
    }

    bool scene_hierarchy::update(const window_update_context&)
    {
        bool open{true};

        if (ImGui::Begin("Hierarchy", &open))
        {
            for (const auto e : m_registry->entities())
            {
                ImGuiTreeNodeFlags flags{ImGuiTreeNodeFlags_Leaf};

                if (m_selection == e)
                {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                auto* name = m_registry->try_get<engine::name_component>(e);

                const auto label = name && !name->value.empty() ? name->value.c_str() : "Unnamed Entity";

                if (ImGui::TreeNodeEx(reinterpret_cast<void*>(e.value), flags, "%s", label))
                {
                    // static bool selection[5] = {false, false, false, false, false};
                    // for (int n = 0; n < 5; n++)
                    // {
                    //     char buf[32];
                    //     sprintf(buf, "Object %d", n);
                    //     if (ImGui::Selectable(buf, selection[n]))
                    //     {
                    //         if (!ImGui::GetIO().KeyCtrl)
                    //         {
                    //             memset(selection, 0, sizeof(selection));
                    //         }

                    //         selection[n] ^= 1; // Toggle current item
                    //     }
                    // }

                    ImGui::TreePop();
                }

                if (ImGui::IsItemClicked())
                {
                    // m_selection = e;
                }
            }

            ImGui::End();
        }

        return open;
    }
}