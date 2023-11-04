#include <oblo/editor/windows/scene_hierarchy.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/editor/utility/entity_utility.hpp>
#include <oblo/editor/window_update_context.hpp>

#include <imgui.h>

namespace oblo::editor
{
    void scene_hierarchy::init(const window_update_context& ctx)
    {
        m_registry = ctx.services.find<ecs::entity_registry>();
        m_selection = ctx.services.find<selected_entities>();
    }

    bool scene_hierarchy::update(const window_update_context&)
    {
        bool open{true};

        if (ImGui::Begin("Hierarchy", &open))
        {
            for (const auto e : m_registry->entities())
            {
                ImGuiTreeNodeFlags flags{ImGuiTreeNodeFlags_Leaf};

                if (m_selection->contains(e))
                {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                auto* const name = entity_utility::get_name_cstr(*m_registry, e);

                if (ImGui::TreeNodeEx(reinterpret_cast<void*>(e.value), flags, "%s", name))
                {
                    ImGui::TreePop();
                }

                if (ImGui::IsItemClicked())
                {
                    if (!ImGui::GetIO().KeyCtrl)
                    {
                        m_selection->clear();
                    }

                    m_selection->add(e);
                }
            }

            ImGui::End();
        }

        return open;
    }
}