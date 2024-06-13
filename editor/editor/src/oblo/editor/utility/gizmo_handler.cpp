#include <oblo/editor/utility/gizmo_handler.hpp>

#include <oblo/core/debug.hpp>
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

namespace oblo::editor::gizmo_handler
{
    namespace
    {
        void imguizmo_frustum(float left, float right, float bottom, float top, float znear, float zfar, float* m16)
        {
            f32 temp, temp2, temp3, temp4;
            temp = 2.0f * znear;
            temp2 = right - left;
            temp3 = top - bottom;
            temp4 = zfar - znear;
            m16[0] = temp / temp2;
            m16[1] = 0.0;
            m16[2] = 0.0;
            m16[3] = 0.0;
            m16[4] = 0.0;
            m16[5] = temp / temp3;
            m16[6] = 0.0;
            m16[7] = 0.0;
            m16[8] = (right + left) / temp2;
            m16[9] = (top + bottom) / temp3;
            m16[10] = (-zfar - znear) / temp4;
            m16[11] = -1.0f;
            m16[12] = 0.0;
            m16[13] = 0.0;
            m16[14] = (-temp * zfar) / temp4;
            m16[15] = 0.0;
        }

        void imguizmo_perspective(float fovyInDegrees, float aspectRatio, float znear, float zfar, float* m16)
        {
            float ymax, xmax;
            ymax = znear * std::tanf(fovyInDegrees * 3.141592f / 180.0f);
            xmax = ymax * aspectRatio;
            imguizmo_frustum(-xmax, xmax, -ymax, ymax, znear, zfar, m16);
        }
    }

    bool handle_transform_gizmos(ecs::entity_registry& reg,
        std::span<const ecs::entity> entities,
        vec2 origin,
        vec2 size,
        const ecs::entity cameraEntity)
    {
        ImGuizmo::BeginFrame();

        ImGuizmo::AllowAxisFlip(true);

        ImGuizmo::SetOrthographic(false);
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

        // TODO: Maybe ignore all editor entities?
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

        const mat4 view = *inverse(cameraWorld.value);

        const f32 ratio = f32(size.y) / size.x;

        mat4 projection = make_perspective_matrix(camera.fovy, ratio, camera.near, camera.far);

        // Flip Y to change handedness
        projection.at(1, 1) = -projection.at(1, 1);

        f32* matrix = &transformComp->value.at(0, 0);

        const auto interacting =
            ImGuizmo::Manipulate(&view.at(0, 0), &projection.at(0, 0), ImGuizmo::TRANSLATE, ImGuizmo::WORLD, matrix);

        if (interacting)
        {
            vec3 translation;
            vec3 rotation;
            vec3 scale;

            ImGuizmo::DecomposeMatrixToComponents(matrix, &translation.x, &rotation.x, &scale.x);

            // We are only transforming the position for now
            positionComp->value = translation;
        }

        return interacting || ImGuizmo::IsUsing();
    }
}