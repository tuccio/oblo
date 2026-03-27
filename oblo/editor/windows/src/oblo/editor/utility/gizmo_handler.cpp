#include <oblo/editor/utility/gizmo_handler.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/pair.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/gpu_components.hpp>
#include <oblo/graphics/components/mesh_internal.hpp>
#include <oblo/graphics/components/skin_component.hpp>
#include <oblo/math/transform.hpp>
#include <oblo/math/view_projection.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/components/parent_component.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>
#include <oblo/scene/resources/skeleton.hpp>

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

        void draw_joints(
            const resource_registry& resources, ecs::entity_registry& reg, ecs::entity e, const mat4 viewProj)
        {
            auto* const transformCompPtr = reg.try_get<global_transform_component>(e);
            auto* const skinningChunksPtr = reg.try_get<joint_skinning_transform_chunks_component>(e);

            if (!transformCompPtr || !skinningChunksPtr)
            {
                return;
            }

            global_transform_component& transformComp = *transformCompPtr;
            joint_skinning_transform_chunks_component& skinningChunks = *skinningChunksPtr;

            const skin_component* skinComp = reg.try_get<skin_component>(e);

            resource_ptr<skin> skinPtr;

            if (skinComp)
            {
                skinPtr = resources.get_resource(skinComp->skin);
                skinPtr.load_sync();

                if (!skinPtr.is_successfully_loaded())
                {
                    skinPtr = {};
                }
            }

            dynamic_array<mat4> transforms;
            transforms.resize_default(skinningChunks.numJoints);

            const auto project2d = [&viewProj, pos = ImGui::GetWindowPos(), size = ImGui::GetWindowSize()](
                                       const vec4& p) -> vec2
            {
                const vec4 projected = viewProj * p;
                const vec2 ndc = vec2{projected.x, projected.y} / projected.w;

                return vec2{
                    pos.x + (ndc.x + 1.f) * .5f * size.x,
                    pos.y + (1.f - (ndc.y + 1.f) * .5f) * size.y,
                };
            };

            auto* const drawList = ImGui::GetWindowDrawList();

            string_builder nameBuilder;

            for (u32 jointIndex = 0; jointIndex < skinningChunks.numJoints; ++jointIndex)
            {
                const u32 chunkIndex = jointIndex / joint_pose_component::joints_per_chunk;
                const u32 jointLocalIndex = jointIndex % joint_pose_component::joints_per_chunk;

                const ecs::entity chunkEntity = skinningChunks.chunks[chunkIndex];
                const joint_pose_component* pose = reg.try_get<joint_pose_component>(chunkEntity);

                if (pose)
                {
                    const auto& localPose = pose->localPoses[jointLocalIndex];

                    const mat4 localMatrix =
                        make_transform_matrix(localPose.translation, localPose.rotation, localPose.scale);

                    const u32 parentIndex = pose->parentJointIndices[jointLocalIndex];

                    const mat4* parentTransform = &transformComp.localToWorld;

                    if (parentIndex != joint_pose_component::no_parent)
                    {
                        parentTransform = &transforms[parentIndex];
                    }

                    const mat4 worldMatrix = *parentTransform * localMatrix;
                    transforms[jointIndex] = worldMatrix;

                    const vec2 previousPoint = project2d(*parentTransform * vec4(0, 0, 0, 1));
                    const vec2 nextPoint = project2d(worldMatrix * vec4(0, 0, 0, 1));

                    constexpr f32 jointThickness = 4.f;

                    static constexpr ImU32 colors[13] = {
                        IM_COL32(99, 110, 250, 255),  // Blue (#636EFA)
                        IM_COL32(239, 85, 59, 255),   // Red (#EF553B)
                        IM_COL32(0, 204, 150, 255),   // Green (#00CC96)
                        IM_COL32(171, 99, 250, 255),  // Purple (#AB63FA)
                        IM_COL32(255, 161, 90, 255),  // Orange (#FFA15A)
                        IM_COL32(25, 211, 243, 255),  // Cyan (#19D3F3)
                        IM_COL32(255, 102, 146, 255), // Pink (#FF6692)
                        IM_COL32(182, 232, 128, 255), // Light Green (#B6E880)
                        IM_COL32(255, 151, 255, 255), // Light Pink (#FF97FF)
                        IM_COL32(254, 203, 82, 255),  // Yellow (#FECB52)
                        IM_COL32(31, 119, 180, 255),  // Dark Blue (#1F77B4)
                        IM_COL32(23, 190, 207, 255),  // Teal (#17BECF)
                        IM_COL32(157, 0, 255, 255)    // Violet (#9D00FF)
                    };

                    const auto jointColor = colors[jointIndex % array_size(colors)];

                    drawList->AddLine({previousPoint.x, previousPoint.y},
                        {nextPoint.x, nextPoint.y},
                        jointColor,
                        jointThickness);

                    if (skinPtr)
                    {
                        const vec2 jointDirection = normalize(nextPoint - previousPoint);
                        const vec2 textPos = nextPoint - jointDirection * vec2{16.f, 16.f};

                        nameBuilder.clear().format("{} [{}]", skinPtr->jointNames[jointIndex], jointIndex);
                        drawList->AddText({textPos.x, textPos.y}, jointColor, nameBuilder.begin(), nameBuilder.end());
                    }
                }
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

    bool gizmo_handler::handle(const resource_registry& resources,
        ecs::entity_registry& reg,
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
                mat4 globalJointTransform = jointTransforms.jointMatrices[jointIndex] *
                    inverse(poseComp.invBindPoses[jointIndex]).value_or(mat4::identity());

                f32* const matrix = &globalJointTransform.at(0, 0);

                ImGuizmo::SetID(int(m_id + jointIndex));

                f32 deltaMatrix[16];

                const bool jointInteracting = ImGuizmo::Manipulate(&view.at(0, 0),
                    &projection.at(0, 0),
                    get_imguizmo_operation(m_op),
                    ImGuizmo::WORLD,
                    matrix,
                    deltaMatrix);

                if (jointInteracting)
                {
                    interacting = true;

                    auto& jointPose = poseComp.localPoses[jointIndex];
                    update_trs(m_op, deltaMatrix, jointPose.translation, jointPose.rotation, jointPose.scale);
                    reg.notify(e);

                    break;
                }
            }

            if (auto* const parent = reg.try_get<parent_component>(e))
            {
                draw_joints(resources, reg, parent->parent, projection * view);
            }
        }

        if (!interacting && reg.has<joint_skinning_transform_chunks_component>(e))
        {
            const auto [view, projection] = calculateViewProjection();
            draw_joints(resources, reg, e, projection * view);
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