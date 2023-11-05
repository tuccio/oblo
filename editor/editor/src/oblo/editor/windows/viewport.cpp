#include <oblo/editor/windows/viewport.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/engine/components/name_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>

#include <imgui.h>

namespace oblo::editor
{
    void viewport::init(const window_update_context& ctx)
    {
        m_entities = ctx.services.find<ecs::entity_registry>();
        OBLO_ASSERT(m_entities);
    }

    bool viewport::update(const window_update_context&)
    {
        bool open{true};

        if (ImGui::Begin("Viewport", &open))
        {
            const auto regionMin = ImGui::GetWindowContentRegionMin();
            const auto regionMax = ImGui::GetWindowContentRegionMax();

            const auto windowSize = ImVec2{regionMax.x - regionMin.x, regionMax.y - regionMin.y};

            if (!m_entity)
            {
                m_entity = m_entities->create<viewport_component, name_component>();
                auto& name = m_entities->get<name_component>(m_entity);
                name.name = "Editor Camera";
            }

            auto& v = m_entities->get<viewport_component>(m_entity);

            v.width = u32(windowSize.x);
            v.height = u32(windowSize.y);

            if (auto const imageId = v.imageId)
            {
                ImGui::Image(imageId, windowSize);
            }

            ImGui::End();
        }

        return open;
    }
}