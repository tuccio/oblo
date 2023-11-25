#include <oblo/editor/windows/viewport.hpp>

#include <oblo/core/zip_range.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>
#include <oblo/editor/data/drag_and_drop_payload.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/input/input_queue.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

#include <imgui.h>

#include <format>

namespace oblo::editor
{
    void viewport::init(const window_update_context& ctx)
    {
        m_entities = ctx.services.find<ecs::entity_registry>();
        OBLO_ASSERT(m_entities);

        m_resources = ctx.services.find<resource_registry>();
        OBLO_ASSERT(m_resources);

        m_inputQueue = ctx.services.find<const input_queue>();
        OBLO_ASSERT(m_inputQueue);

        using action = fps_camera_controller::action;
        m_cameraController.clear_bindings();

        m_cameraController.bind(mouse_key::right, action::mouse_look);

        m_cameraController.bind(keyboard_key::w, action::strafe_forward);
        m_cameraController.bind(keyboard_key::a, action::strafe_left);
        m_cameraController.bind(keyboard_key::s, action::strafe_backward);
        m_cameraController.bind(keyboard_key::d, action::strafe_right);
        m_cameraController.bind(keyboard_key::e, action::strafe_upward);
        m_cameraController.bind(keyboard_key::q, action::strafe_downward);
    }

    bool viewport::update(const window_update_context& ctx)
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
                    m_cameraController.get_position(),
                    m_cameraController.get_orientation(),
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

                if (ImGui::IsWindowFocused() && m_entity)
                {
                    const auto [w, h] = ImGui::GetItemRectSize();
                    m_cameraController.set_screen_size({w, h});
                    m_cameraController.process(m_inputQueue->get_events());

                    auto& p = m_entities->get<position_component>(m_entity);
                    p.value = m_cameraController.get_position();

                    auto& r = m_entities->get<rotation_component>(m_entity);
                    r.value = m_cameraController.get_orientation();
                }

                if (ImGui::BeginDragDropTarget())
                {
                    if (auto* const payload = ImGui::AcceptDragDropPayload(payloads::Resource))
                    {
                        const uuid id = payloads::parse_uuid(payload->Data);
                        const auto resource = m_resources->get_resource(id);

                        if (const resource_ptr modelRes = resource.as<model>())
                        {
                            for (const auto& [mesh, material] : zip_range(modelRes->meshes, modelRes->materials))
                            {
                                if (const resource_ptr meshRes = m_resources->get_resource(mesh.id))
                                {
                                    const auto name = meshRes.get_name();

                                    const auto e =
                                        ecs_utility::create_named_physical_entity<static_mesh_component>(*m_entities,
                                            name.empty() ? "New Mesh" : name,
                                            vec3{},
                                            quaternion::identity(),
                                            vec3::splat(1));

                                    auto& sm = m_entities->get<static_mesh_component>(e);
                                    sm.mesh = mesh;
                                    sm.material = material;

                                    auto* const selected = ctx.services.find<selected_entities>();

                                    if (selected)
                                    {
                                        selected->clear();
                                        selected->add({&e, 1});
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ImGui::End();
        }

        return open;
    }
}