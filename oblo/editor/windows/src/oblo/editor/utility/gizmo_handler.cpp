#include <oblo/editor/utility/gizmo_handler.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/pair.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/gpu_components.hpp>
#include <oblo/graphics/components/mesh_internal.hpp>
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

        void update_trs(gizmo_handler::operation op, const f32* matrix, vec3& outT, quaternion& outR, vec3& outS)
        {
            vec3 translation;
            vec3 rotation;
            vec3 scale;

            ImGuizmo::DecomposeMatrixToComponents(matrix, &translation.x, &rotation.x, &scale.x);

            switch (op)
            {
            case gizmo_handler::operation::translation:
                outT = translation;
                break;

            case gizmo_handler::operation::rotation: {
                std::swap(rotation.x, rotation.z);
                const auto q = quaternion::from_euler_zyx_intrinsic(degrees_tag{}, rotation);
                outR = q;
            }

            break;

            case gizmo_handler::operation::scale:
                outS = scale;
                break;

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

        const auto calculateViewProjection = [&reg, cameraEntity, size]() -> pair<mat4, mat4>
        {
            const auto& [camera, cameraWorld] = reg.get<camera_component, global_transform_component>(cameraEntity);

            const mat4 view = inverse(cameraWorld.localToWorld).assert_value_or(mat4::identity());

            const f32 ratio = f32(size.y) / size.x;

            mat4 projection = make_perspective_matrix(camera.fovy, ratio, camera.near, camera.far);

            // Flip Y to change handedness
            projection.at(1, 1) = -projection.at(1, 1);

            return {view, projection};
        };

        bool interacting = false;

        if (!interacting && reg.has<joint_pose_component, joint_skinning_transform_component>(e))
        {
            const auto [view, projection] = calculateViewProjection();

            auto&& [poseComp, jointTransforms] = reg.get<joint_pose_component, joint_skinning_transform_component>(e);

            static_assert(
                joint_pose_component::joints_per_chunk == joint_skinning_transform_component::joints_per_chunk);

            for (u32 jointIndex = 0; jointIndex < joint_pose_component::joints_per_chunk; ++jointIndex)
            {
                f32* const matrix = &jointTransforms.jointMatrices[jointIndex].at(0, 0);

                ImGuizmo::SetID(int(m_id + jointIndex));

                const bool jointInteracting = ImGuizmo::Manipulate(&view.at(0, 0),
                    &projection.at(0, 0),
                    get_imguizmo_operation(m_op),
                    ImGuizmo::WORLD,
                    matrix);

                if (jointInteracting)
                {
                    interacting = true;

                    /*auto& jointPose = poseComp.currentPoses[jointIndex];
                    update_trs(m_op, matrix, jointPose.translation, jointPose.rotation, jointPose.scale);*/
                    reg.notify(e);

                    break;
                }
            }
        }

        if (!interacting &&
            reg.has<position_component, rotation_component, scale_component, global_transform_component>(e))
        {
            const auto [view, projection] = calculateViewProjection();

            auto&& [positionComp, rotationComp, scaleComp, transformComp] =
                reg.get<position_component, rotation_component, scale_component, global_transform_component>(e);

            f32* const matrix = &transformComp.localToWorld.at(0, 0);

            ImGuizmo::SetID(int(m_id));

            interacting = ImGuizmo::Manipulate(&view.at(0, 0),
                &projection.at(0, 0),
                get_imguizmo_operation(m_op),
                ImGuizmo::WORLD,
                matrix);

            if (interacting)
            {
                update_trs(m_op, matrix, positionComp.value, rotationComp.value, scaleComp.value);
                reg.notify(e);
            }
        }

        return interacting || ImGuizmo::IsUsing();
    }
}