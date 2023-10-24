#include <oblo/editor/windows/viewport.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>
#include <oblo/graphics/components/viewport_component.hpp>

#include <imgui.h>

namespace oblo::editor
{
    viewport::viewport(ecs::entity_registry& entities) : m_entities{&entities} {}

    viewport::~viewport() = default;

    bool viewport::update()
    {
        bool open{true};

        if (ImGui::Begin("Viewport", &open))
        {
            const auto regionMin = ImGui::GetWindowContentRegionMin();
            const auto regionMax = ImGui::GetWindowContentRegionMax();

            const auto windowSize = ImVec2{regionMax.x - regionMin.x, regionMax.y - regionMin.y};

            if (!m_entity)
            {
                m_entity = m_entities->create<graphics::viewport_component>();
            }

            auto& v = m_entities->get<graphics::viewport_component>(m_entity);

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