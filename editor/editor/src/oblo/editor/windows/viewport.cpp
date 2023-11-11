#include <oblo/editor/windows/viewport.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/engine/utility/ecs_utility.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>

#include <imgui.h>

#include <format>

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
                m_entity = ecs_utility::create_named_physical_entity<camera_component, viewport_component>(*m_entities,
                    "Editor Camera",
                    vec3{},
                    quaternion::identity(),
                    vec3::splat(1));

                auto& camera = m_entities->get<camera_component>(m_entity);
                camera.near = 0.01f;
                camera.far = 1000.f;
                camera.fovy = 75_deg;
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