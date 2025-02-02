#include <oblo/editor/utility/gizmo_handler.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/math/view_projection.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>

#include <imgui.h>

#include <ImGuizmo.h>

#include <cmath>

namespace oblo::editor
{
    namespace
    {
        auto get_imguizmo_operation(gizmo_handler::operation op)
        {
            switch (op)
            {
            case gizmo_handler::operation::translation:
                return ImGuizmo::TRANSLATE;

            case gizmo_handler::operation::rotation:
                return ImGuizmo::ROTATE;

            case gizmo_handler::operation::scale:
                return ImGuizmo::SCALE;

            default:
                unreachable();
            }
        }
    }

    void gizmo_handler::set_id(u32 id)
    {
        m_id = id;
    }

    gizmo_handler::operation gizmo_handler::get_operation() const
    {
        return m_op;
    }

    void gizmo_handler::set_operation(operation op)
    {
        m_op = op;
    }

    bool gizmo_handler::handle(ecs::entity_registry& reg,
        std::span<const ecs::entity> entities,
        vec2 origin,
        vec2 size,
        const ecs::entity cameraEntity)
    {
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::AllowAxisFlip(false);

        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(origin.x, origin.y, size.x, size.y);

        if (!cameraEntity)
        {
            OBLO_ASSERT(cameraEntity);
            return false;
        }

        if (entities.size() != 1)
        {
            return false;
        }

        const auto e = entities[0];

        // TODO (#60): Maybe ignore all editor entities?
        if (e == cameraEntity)
        {
            return false;
        }

        if (!reg.contains(e))
        {
            return false;
        }

        auto* positionComp = reg.try_get<position_component>(e);
        auto* rotationComp = reg.try_get<rotation_component>(e);
        auto* scaleComp = reg.try_get<scale_component>(e);
        auto* transformComp = reg.try_get<global_transform_component>(e);

        if (!positionComp || !rotationComp || !scaleComp || !transformComp)
        {
            return false;
        }

        const auto& camera = reg.get<camera_component>(cameraEntity);
        const auto& cameraWorld = reg.get<global_transform_component>(cameraEntity);

        const mat4 view = *inverse(cameraWorld.localToWorld);

        const f32 ratio = f32(size.y) / size.x;

        mat4 projection = make_perspective_matrix(camera.fovy, ratio, camera.near, camera.far);

        // Flip Y to change handedness
        projection.at(1, 1) = -projection.at(1, 1);

        f32* matrix = &transformComp->localToWorld.at(0, 0);

        ImGuizmo::SetID(int(m_id));

        const auto interacting = ImGuizmo::Manipulate(&view.at(0, 0),
            &projection.at(0, 0),
            get_imguizmo_operation(m_op),
            ImGuizmo::WORLD,
            matrix);

        if (interacting)
        {
            vec3 translation;
            vec3 rotation;
            vec3 scale;

            ImGuizmo::DecomposeMatrixToComponents(matrix, &translation.x, &rotation.x, &scale.x);

            switch (m_op)
            {
            case gizmo_handler::operation::translation:
                positionComp->value = translation;
                break;

            case gizmo_handler::operation::rotation: {
                std::swap(rotation.x, rotation.z);
                const auto q = quaternion::from_euler_zyx_intrinsic(degrees_tag{}, rotation);
                rotationComp->value = q;
            }

            break;

            case gizmo_handler::operation::scale:
                scaleComp->value = scale;
                break;

            default:
                unreachable();
            }

            reg.notify(e);
        }

        return interacting || ImGuizmo::IsUsing();
    }
}