#include <oblo/editor/windows/viewport.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/type_set.hpp>
#include <oblo/editor/data/drag_and_drop_payload.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/incremental_id_pool.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/skybox_component.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/input/input_queue.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/tags.hpp>
#include <oblo/scene/resources/model.hpp>
#include <oblo/scene/resources/texture.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

#include <imgui.h>

namespace oblo::editor
{
    namespace
    {
        constexpr f32 SpawnDistance{1.f};
    }

    viewport::~viewport()
    {
        on_close();
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

        m_idPool = ctx.services.find<incremental_id_pool>();
        OBLO_ASSERT(m_idPool);
        m_viewportId = m_idPool->acquire<viewport>();

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

        string_builder buffer;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

        if (ImGui::Begin(buffer.format("Viewport##{}", m_viewportId).c_str(), &open))
        {
            buffer.clear();

            const auto regionMin = ImGui::GetWindowContentRegionMin();
            const auto regionMax = ImGui::GetWindowContentRegionMax();

            const auto viewportPos = ImGui::GetWindowPos();
            const auto windowSize = ImVec2{regionMax.x - regionMin.x, regionMax.y - regionMin.y};

            if (!m_entity)
            {
                m_entity =
                    ecs_utility::create_named_physical_entity<camera_component, viewport_component, transient_tag>(
                        *m_entities,
                        buffer.format("Viewport Camera #{}", m_viewportId).view(),
                        {},
                        m_cameraController.get_position(),
                        m_cameraController.get_orientation(),
                        vec3::splat(1));

                auto& camera = m_entities->get<camera_component>(m_entity);
                camera.near = 0.01f;
                camera.far = 10000.f;
                camera.fovy = 75_deg;
            }

            auto& v = m_entities->get<viewport_component>(m_entity);

            v.width = u32(max(1.f, windowSize.x));
            v.height = u32(max(1.f, windowSize.y));

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
                    if (!gizmoActive && ImGui::IsItemClicked())
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

                        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                        {
                            m_selection->clear();
                        }
                    }

                    auto&& [p, r] = m_entities->get<position_component, rotation_component>(m_entity);

                    const auto [w, h] = ImGui::GetItemRectSize();
                    m_cameraController.set_screen_size({w, h});
                    m_cameraController.process(m_inputQueue->get_events(), m_timeStats->dt);

                    p.value = m_cameraController.get_position();
                    r.value = m_cameraController.get_orientation();

                    m_entities->notify(m_entity);
                }

                if (ImGui::BeginDragDropTarget())
                {
                    if (auto* const artifactPayload = ImGui::AcceptDragDropPayload(payloads::Artifact))
                    {
                        const uuid id = payloads::unpack_artifact(artifactPayload->Data);
                        spawn_artifact(ctx, id);
                    }
                    else if (auto* const assetPayload = ImGui::AcceptDragDropPayload(payloads::Asset))
                    {
                        const uuid id = payloads::unpack_asset(assetPayload->Data);

                        asset_meta assetMeta;
                        artifact_meta artifactMeta;

                        if (auto* assets = ctx.services.find<asset_registry>())
                        {
                            if (assets->find_asset_by_id(id, assetMeta) &&
                                assets->find_artifact_by_id(assetMeta.mainArtifactHint, artifactMeta))
                            {
                                spawn_artifact(ctx, artifactMeta.artifactId);
                            }
                        }
                    }

                    ImGui::EndDragDropTarget();
                }
            }

            ImGui::SetWindowFontScale(.95f);

            constexpr f32 viewportModeWidth = 128.f;
            constexpr f32 padding = 2.f;

            ImGui::SetCursorPosX(regionMax.x - viewportModeWidth - padding);
            ImGui::SetCursorPosY(topLeft.y + padding);

            ImGui::SetNextItemWidth(viewportModeWidth);

            if (ImGui::BeginCombo("##viewport_mode",
                    m_viewportModes[usize(v.mode)].c_str(),
                    ImGuiComboFlags_HeightLarge))
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

        ImGui::PopStyleVar(2);

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

        if (m_idPool)
        {
            m_idPool->release<viewport>(m_viewportId);
            m_idPool = {};
        }
    }

    void viewport::spawn_artifact(const window_update_context& ctx, uuid id)
    {
        const auto resource = m_resources->get_resource(id);

        if (const resource_ptr modelRes = resource.as<model>())
        {
            // TODO: This is not how we want to spawn stuff, rather create a component with a reference
            modelRes.load_sync();

            for (const auto& [mesh, material] : zip_range(modelRes->meshes, modelRes->materials))
            {
                if (const resource_ptr meshRes = m_resources->get_resource(mesh.id))
                {
                    const auto name = meshRes.get_name();

                    const auto e = ecs_utility::create_named_physical_entity<static_mesh_component>(*m_entities,
                        name.empty() ? "New Mesh" : name,
                        {},
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
        else if (const resource_ptr textureRes = resource.as<texture>())
        {
            const auto name = textureRes.get_name();

            const auto e = ecs_utility::create_named_physical_entity<skybox_component>(*m_entities,
                name.empty() ? "New Skybox" : name,
                {},
                vec3{},
                quaternion::identity(),
                vec3::splat(1));

            auto& sm = m_entities->get<skybox_component>(e);
            sm.texture = textureRes.as_ref();
            sm.multiplier = 1.f;
            sm.tint = vec3::splat(1.f);

            auto* const selected = ctx.services.find<selected_entities>();

            if (selected)
            {
                selected->clear();
                selected->add({&e, 1});
            }
        }
    }
}
