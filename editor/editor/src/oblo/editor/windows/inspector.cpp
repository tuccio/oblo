#include <oblo/editor/windows/inspector.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/ecs/component_type_desc.hpp>
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

            for (const auto e : selectedEntities)
            {
                if (e && m_registry->contains(e))
                {
                    auto* const name = entity_utility::get_name_cstr(*m_registry, e);
                    ImGui::TextUnformatted(name);

                    const auto& typeRegistry = m_registry->get_type_registry();
                    const std::span components = m_registry->get_component_types(e);

                    for (const ecs::component_type type : components)
                    {
                        const auto& desc = typeRegistry.get_component_type_desc(type);

                        char name[128];
                        const auto length = min<usize>(array_size(name) - 1, desc.type.name.size());
                        std::memcpy(name, desc.type.name.data(), length);

                        name[length] = '\0';

                        ImGui::CollapsingHeader(name);
                    }

                    // Just pick the first entity for now
                    break;
                }
            }

            ImGui::End();
        }

        return open;
    }
}