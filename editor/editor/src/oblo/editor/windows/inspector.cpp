#include <oblo/editor/windows/inspector.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/editor/utility/entity_utility.hpp>
#include <oblo/editor/window_update_context.hpp>

#include <imgui.h>

namespace oblo::editor
{
    void inspector::init(const window_update_context& ctx)
    {
        m_registry = ctx.services.find<ecs::entity_registry>();
        m_selection = ctx.services.find<selected_entities>();
    }

    bool inspector::update(const window_update_context&)
    {
        bool open{true};

        if (ImGui::Begin("Inspector", &open))
        {
            const std::span selectedEntities = m_selection->get();

            // Just pick the first entity for now
            if (!selectedEntities.empty())
            {
                const auto e = selectedEntities[0];

                if (e && m_registry->contains(e))
                {
                    auto* const name = entity_utility::get_name_cstr(*m_registry, e);
                    ImGui::TextUnformatted(name);
                }
            }

            ImGui::End();
        }

        return open;
    }
}