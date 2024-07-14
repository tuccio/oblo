#include <oblo/editor/windows/viewport.hpp>

#include <oblo/core/iterator/zip_range.hpp>
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
#include <oblo/reflection/reflection_registry.hpp>
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
    namespace
    {
        u32 s_viewportInstances{};

        constexpr f32 SpawnDistance{1.f};
    }

    viewport::~viewport()
    {
        on_close();

        --s_viewportInstances;
    }

    void viewport::init(const window_update_context& ctx)
    {
        m_entities = ctx.services.find<ecs::entity_registry>();
        OBLO_ASSERT(m_entities);

        m_resources = ctx.services.find<resource_registry>();
        OBLO_ASSERT(m_resources);

        m_inputQueue = ctx.services.find<const input_queue>();
        OBLO_ASSERT(m_inputQueue);

        m_selection = ctx.services.find<selected_entities>();
        OBLO_ASSERT(m_selection);

        m_timeStats = ctx.services.find<const time_stats>();
        OBLO_ASSERT(m_timeStats);

        m_viewportId = s_viewportInstances++;

        auto* const reflection = ctx.services.find<const reflection::reflection_registry>();
        const auto viewportMode = reflection->find_enum<viewport_mode>();

        const std::span viewportModeNames = reflection->get_enumerator_names(viewportMode);
        const std::span viewportModeValues = reflection->get_enumerator_values(viewportMode);

        m_viewportModes.assign(viewportModeNames.size(), {});

        for (usize i = 0; i < viewportModeNames.size(); ++i)
        {
            std::underlying_type_t<viewport_mode> value;
            std::memcpy(&value, viewportModeValues.data() + sizeof(viewport_mode) * i, sizeof(viewport_mode));

            m_viewportModes[value] = viewportModeNames[i];
        }
    }

    bool viewport::update(const window_update_context& ctx)
    {
        bool open{true};

        char buffer[64];
        *std::format_to(buffer, "Viewport##{}", m_viewportId) = '\0';

        if (ImGui::Begin(buffer, &open))
        {
            const auto regionMin = ImGui::GetWindowContentRegionMin();
            const auto regionMax = ImGui::GetWindowContentRegionMax();

            const auto viewportPos = ImGui::GetWindowPos();
            const auto windowSize = ImVec2{regionMax.x - regionMin.x, regionMax.y - regionMin.y};

            if (!m_entity)
            {
                *std::format_to(buffer, "Viewport Camera #{}", m_viewportId) = '\0';

                m_entity = ecs_utility::create_named_physical_entity<camera_component, viewport_component>(*m_entities,
                    buffer,
                    m_cameraController.get_position(),
                    m_cameraController.get_orientation(),
                    vec3::splat(1));

                auto& camera = m_entities->get<camera_component>(m_entity);
                camera.near = 0.01f;
                camera.far = 10000.f;
                camera.fovy = 75_deg;
            }

            auto& v = m_entities->get<viewport_component>(m_entity);

            v.width = u32(windowSize.x);
            v.height = u32(windowSize.y);

            const auto topLeft = ImGui::GetCursorPos();

            if (auto const imageId = v.imageId)
            {
                const bool hasFocus = ImGui::IsWindowFocused();

                ImGui::Image(imageId, windowSize);

                // Maybe use item size?
                m_gizmoHandler.set_id(m_viewportId);

                const auto gizmoActive = m_gizmoHandler.handle(*m_entities,
                    m_selection->get(),
                    {viewportPos.x, viewportPos.y},
                    {windowSize.x, windowSize.y},
                    m_entity);

                switch (v.picking.state)
                {
                case picking_request::state::none:
                    if (hasFocus && !gizmoActive && ImGui::IsItemClicked())
                    {
                        const auto [viewportX, viewportY] = ImGui::GetItemRectMin();
                        const auto [mouseX, mouseY] = ImGui::GetMousePos();
                        v.picking.coordinates = {mouseX - viewportX, mouseY - viewportY};
                        v.picking.state = picking_request::state::requested;
                    }

                    break;

                case picking_request::state::served: {
                    m_selection->clear();

                    if (const ecs::entity selectedEntity{v.picking.result};
                        selectedEntity && m_entities->contains(selectedEntity))
                    {
                        m_selection->add({&selectedEntity, 1});
                    }

                    v.picking.state = picking_request::state::none;
                    break;
                }

                break;

                case picking_request::state::failed: {
                    v.picking.state = picking_request::state::none;
                    break;
                }

                default:
                    break;
                }

                if (hasFocus)
                {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
                    {
                        m_cameraController.set_common_wasd_bindings();
                    }
                    else
                    {
                        m_cameraController.clear_bindings();
                        m_cameraController.reset_actions();

                        if (!gizmoActive)
                        {
                            if (ImGui::IsKeyPressed(ImGuiKey_W))
                            {
                                m_gizmoHandler.set_operation(gizmo_handler::operation::translation);
                            }

                            if (ImGui::IsKeyPressed(ImGuiKey_E))
                            {
                                m_gizmoHandler.set_operation(gizmo_handler::operation::rotation);
                            }

                            if (ImGui::IsKeyPressed(ImGuiKey_R))
                            {
                                m_gizmoHandler.set_operation(gizmo_handler::operation::scale);
                            }
                        }
                    }

                    auto& p = m_entities->get<position_component>(m_entity);
                    auto& r = m_entities->get<rotation_component>(m_entity);

                    const auto [w, h] = ImGui::GetItemRectSize();
                    m_cameraController.set_screen_size({w, h});
                    m_cameraController.process(m_inputQueue->get_events(), m_timeStats->dt);

                    p.value = m_cameraController.get_position();

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

            ImGui::SetWindowFontScale(.9f);

            constexpr f32 viewportModeWidth = 128.f;
            constexpr f32 padding = 2.f;

            ImGui::SetCursorPosX(regionMax.x - viewportModeWidth - padding);
            ImGui::SetCursorPosY(topLeft.y + padding);

            ImGui::SetNextItemWidth(viewportModeWidth);

            if (ImGui::BeginCombo("##viewport_mode", m_viewportModes[usize(v.mode)].c_str()))
            {
                for (usize i = 0; i < m_viewportModes.size(); ++i)
                {
                    bool isSelected{false};

                    if (ImGui::Selectable(m_viewportModes[i].c_str(), &isSelected))
                    {
                        v.mode = viewport_mode(i);
                    }
                }

                ImGui::EndCombo();
            }
        }

        ImGui::End();

        if (!open)
        {
            on_close();
        }

        return open;
    }

    vec3 viewport::get_spawn_location() const
    {
        if (!m_entity)
        {
            return {};
        }

        return m_entities->get<position_component>(m_entity).value +
            m_entities->get<rotation_component>(m_entity).value * vec3{.z = -SpawnDistance};
    }

    void viewport::on_close()
    {
        if (m_entity)
        {
            m_entities->destroy(m_entity);
            m_entity = {};
        }
    }
}