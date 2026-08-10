#include <oblo/editor/utility/gizmo_handler.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/pair.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/editor/utility/gizmo.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/gpu_components.hpp>
#include <oblo/graphics/components/mesh_internal.hpp>
#include <oblo/graphics/components/skin_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/math/constants.hpp>
#include <oblo/math/plane.hpp>
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

#include <cfloat>
#include <cmath>

namespace oblo::editor
{
    namespace
    {
        constexpr u32 AxisCount = 3;
        constexpr vec3 AxisDirections[AxisCount] = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};

        constexpr f32 AxisLengthPixels = 90.f;
        constexpr f32 AxisThickness = 2.f;
        constexpr f32 ArrowHeadSize = 8.f;
        constexpr f32 HandlePickThreshold = 3.f;
        constexpr f32 PlaneSizePixels = 32.f;
        constexpr f32 RingRadiusPixels = 70.f;
        constexpr f32 RingThickness = 2.5f;
        constexpr u32 RingSegments = 48;
        constexpr f32 ScaleBoxSizePixels = 7.f;

        // Offset applied along the surface normal when snapping the pivot to a surface, to avoid coplanar z-fighting
        constexpr f32 SurfaceSnapOffset{0.01f};

        constexpr ImU32 ActiveColor = IM_COL32(255, 200, 60, 255);
        constexpr ImU32 HoverColor = IM_COL32(255, 255, 255, 255);
        constexpr ImU32 ViewRingColor = IM_COL32(255, 255, 255, 160);
        constexpr u32 PlaneAlpha = 0x50u;

        constexpr ImU32 axis_color(u32 axis)
        {
            switch (axis)
            {
            case 0:
                return IM_COL32(229, 57, 53, 255);
            case 1:
                return IM_COL32(76, 175, 80, 255);
            default:
                return IM_COL32(66, 165, 245, 255);
            }
        }

        constexpr ImU32 with_alpha(ImU32 rgb, u32 alpha)
        {
            return (rgb & 0x00FFFFFFu) | (alpha << 24u);
        }

        constexpr u32 color_channel(ImU32 c, u8 shift)
        {
            return (c >> shift) & 0xFFu;
        }

        ImU32 blend_colors(ImU32 a, ImU32 b)
        {
            return IM_COL32((color_channel(a, IM_COL32_R_SHIFT) + color_channel(b, IM_COL32_R_SHIFT)) / 2,
                (color_channel(a, IM_COL32_G_SHIFT) + color_channel(b, IM_COL32_G_SHIFT)) / 2,
                (color_channel(a, IM_COL32_B_SHIFT) + color_channel(b, IM_COL32_B_SHIFT)) / 2,
                255);
        }

        struct trs
        {
            vec3 position;
            quaternion rotation;
            vec3 scale;
        };

        enum class gizmo_handle_type : u8
        {
            none,
            axis_x,
            axis_y,
            axis_z,
            plane_xy,
            plane_xz,
            plane_yz,
            ring_x,
            ring_y,
            ring_z,
            ring_view,
            center,
        };

        struct gizmo_drag_state
        {
            bool active{};
            bool isJoint{};
            u32 jointIndex{};
            gizmo_handle_type handle{};

            // World-space transform at drag start.
            vec3 startPosition{};
            quaternion startRotation{};
            vec3 startScale{};

            // Local-space transform at drag start.
            vec3 startLocalPosition{};
            quaternion startLocalRotation{};
            vec3 startLocalScale{};

            // Drag plane and grab point.
            vec3 startPivot{};
            vec3 startGrabPoint{};
            vec3 dragPlaneNormal{};

            // Handles specific data.
            vec3 axisDir{};
            vec3 axisDir2{};
            vec3 basisU{};
            vec3 basisV{};
            f32 startAngle{};
            f32 referenceLength{};
        };

        struct gizmo_frame
        {
            vec2 origin;
            vec2 size;
            mat4 viewProj;
            mat4 invViewProj;
            vec3 cameraPosition;
            vec3 cameraForward;
            vec3 pivot;
            f32 pixelsToWorld;
            vec2 mouse;
            ImDrawList* drawList;
        };

        vec2 project_point(const gizmo_frame& frame, const vec3& p)
        {
            return gizmo::world_to_screen(frame.viewProj, frame.origin, frame.size, p);
        }

        ray get_mouse_ray(const gizmo_frame& frame)
        {
            return gizmo::screen_to_world_ray(frame.invViewProj, frame.origin, frame.size, frame.mouse);
        }

        constexpr bool is_axis(gizmo_handle_type h)
        {
            return h >= gizmo_handle_type::axis_x && h <= gizmo_handle_type::axis_z;
        }

        constexpr bool is_ring(gizmo_handle_type h)
        {
            return h >= gizmo_handle_type::ring_x && h <= gizmo_handle_type::ring_view;
        }

        constexpr u32 axis_index(gizmo_handle_type h)
        {
            OBLO_ASSERT(is_axis(h));
            return u32(h) - u32(gizmo_handle_type::axis_x);
        }

        constexpr u32 ring_axis_index(gizmo_handle_type h)
        {
            OBLO_ASSERT(is_ring(h));
            return u32(h) - u32(gizmo_handle_type::ring_x);
        }

        void handle_axes(gizmo_handle_type h, u32& outA, u32& outB)
        {
            switch (h)
            {
            case gizmo_handle_type::plane_xy:
                outA = 0;
                outB = 1;
                break;
            case gizmo_handle_type::plane_xz:
                outA = 0;
                outB = 2;
                break;
            case gizmo_handle_type::plane_yz:
                outA = 1;
                outB = 2;
                break;
            default:
                unreachable();
            }
        }

        vec3 axis_end(const gizmo_frame& frame, u32 axis)
        {
            return frame.pivot + AxisDirections[axis] * (frame.pixelsToWorld * AxisLengthPixels);
        }

        f32 plane_size(const gizmo_frame& frame)
        {
            return frame.pixelsToWorld * PlaneSizePixels;
        }

        gizmo_handle_type pick_translation_handle(const gizmo_frame& frame)
        {
            f32 bestDist = HandlePickThreshold;
            gizmo_handle_type best = gizmo_handle_type::none;

            for (u32 i = 0; i < AxisCount; ++i)
            {
                const f32 dist = gizmo::distance_to_segment(frame.mouse,
                    project_point(frame, frame.pivot),
                    project_point(frame, axis_end(frame, i)));

                if (dist < bestDist)
                {
                    bestDist = dist;
                    best = gizmo_handle_type(u32(gizmo_handle_type::axis_x) + i);
                }
            }

            if (best != gizmo_handle_type::none)
            {
                return best;
            }

            const f32 planeSizeWorld = plane_size(frame);

            f32 bestScore = std::numeric_limits<f32>::lowest();

            for (u32 i = 0; i < AxisCount; ++i)
            {
                const u32 j = (i + 1) % AxisCount;

                const vec2 corners[4] = {
                    project_point(frame, frame.pivot),
                    project_point(frame, frame.pivot + AxisDirections[i] * planeSizeWorld),
                    project_point(frame, frame.pivot + (AxisDirections[i] + AxisDirections[j]) * planeSizeWorld),
                    project_point(frame, frame.pivot + AxisDirections[j] * planeSizeWorld),
                };

                if (!gizmo::point_in_quad(frame.mouse, corners))
                {
                    continue;
                }

                // Plane normal is the axis not used by this plane.
                const vec3 normal = cross(AxisDirections[i], AxisDirections[j]);

                const f32 score = std::abs(dot(normal, frame.cameraForward));

                if (score > bestScore)
                {
                    bestScore = score;

                    switch (i)
                    {
                    case 0:
                        best = gizmo_handle_type::plane_xy;
                        break;
                    case 1:
                        best = gizmo_handle_type::plane_yz;
                        break;
                    default:
                        best = gizmo_handle_type::plane_xz;
                        break;
                    }
                }
            }

            return best;
        }

        void ring_screen_points(const gizmo_frame& frame,
            const vec3& center,
            const vec3& normal,
            f32 radius,
            vec2 (&outPoints)[RingSegments])
        {
            vec3 u = normalize(cross(normal, vec3{.y = 1.f}));

            if (length2(u) < 1e-6f)
            {
                u = normalize(cross(normal, vec3{.x = 1.f}));
            }

            const vec3 v = normalize(cross(normal, u));

            for (u32 i = 0; i < RingSegments; ++i)
            {
                const f32 angle = f32(i) / RingSegments * 2.f * pi;
                const vec3 p = center + u * (std::cos(angle) * radius) + v * (std::sin(angle) * radius);
                outPoints[i] = project_point(frame, p);
            }
        }

        f32 ring_distance(const gizmo_frame& frame, const vec3& center, const vec3& normal, f32 radius)
        {
            vec2 points[RingSegments];
            ring_screen_points(frame, center, normal, radius, points);

            f32 best = FLT_MAX;

            for (u32 i = 0; i < RingSegments; ++i)
            {
                best = min(best, gizmo::distance_to_segment(frame.mouse, points[i], points[(i + 1) % RingSegments]));
            }

            return best;
        }

        gizmo_handle_type pick_rotation_handle(const gizmo_frame& frame)
        {
            const f32 radius = frame.pixelsToWorld * RingRadiusPixels;

            f32 bestDist = HandlePickThreshold;
            gizmo_handle_type best = gizmo_handle_type::none;

            for (u32 i = 0; i < AxisCount; ++i)
            {
                const f32 dist = ring_distance(frame, frame.pivot, AxisDirections[i], radius);

                if (dist < bestDist)
                {
                    bestDist = dist;
                    best = gizmo_handle_type(u32(gizmo_handle_type::ring_x) + i);
                }
            }

            const f32 viewDist =
                ring_distance(frame, frame.pivot, normalize(frame.cameraPosition - frame.pivot), radius);

            if (viewDist < bestDist)
            {
                best = gizmo_handle_type::ring_view;
            }

            return best;
        }

        gizmo_handle_type pick_scale_handle(const gizmo_frame& frame)
        {
            f32 bestDist = HandlePickThreshold;
            gizmo_handle_type best = gizmo_handle_type::none;

            for (u32 i = 0; i < AxisCount; ++i)
            {
                const f32 dist = gizmo::distance_to_segment(frame.mouse,
                    project_point(frame, frame.pivot),
                    project_point(frame, axis_end(frame, i)));

                if (dist < bestDist)
                {
                    bestDist = dist;
                    best = gizmo_handle_type(u32(gizmo_handle_type::axis_x) + i);
                }
            }

            if (best != gizmo_handle_type::none)
            {
                return best;
            }

            if (length(project_point(frame, frame.pivot) - frame.mouse) < ScaleBoxSizePixels)
            {
                return gizmo_handle_type::center;
            }

            return gizmo_handle_type::none;
        }

        void begin_drag(gizmo_drag_state& drag,
            gizmo_handle_type handle,
            bool isJoint,
            u32 jointIndex,
            const gizmo_frame& frame,
            const trs& world,
            const trs& local,
            const vec3& planeNormal,
            const vec3& axisDir,
            const vec3& axisDir2,
            f32 referenceLength)
        {
            drag = {};
            drag.active = true;
            drag.isJoint = isJoint;
            drag.jointIndex = jointIndex;
            drag.handle = handle;
            drag.startPivot = frame.pivot;
            drag.startPosition = world.position;
            drag.startRotation = world.rotation;
            drag.startScale = world.scale;
            drag.startLocalPosition = local.position;
            drag.startLocalRotation = local.rotation;
            drag.startLocalScale = local.scale;
            drag.dragPlaneNormal = planeNormal;
            drag.axisDir = axisDir;
            drag.axisDir2 = axisDir2;
            drag.referenceLength = referenceLength;

            const plane p{planeNormal, -dot(planeNormal, frame.pivot)};

            if (!gizmo::ray_plane_intersection(get_mouse_ray(frame), p, drag.startGrabPoint))
            {
                drag.startGrabPoint = frame.pivot;
            }
        }

        void end_drag(gizmo_drag_state& drag, bool& surfaceSnapping, viewport_component* viewport)
        {
            drag = {};
            surfaceSnapping = false;

            if (viewport)
            {
                viewport->picking.state = picking_request::state::none;
            }
        }

        // Runs the translation gizmo interaction. Returns true while hovering or dragging, and fills out with the new
        // local transform while dragging.
        bool manipulate_translation(gizmo_drag_state& drag,
            gizmo_handle_type& hovered,
            const gizmo_frame& frame,
            bool isJoint,
            u32 jointIndex,
            const trs& world,
            const trs& local,
            trs& out)
        {
            out.rotation = local.rotation;
            out.scale = local.scale;

            if (drag.active)
            {
                if (drag.isJoint != isJoint || drag.jointIndex != jointIndex)
                {
                    return false;
                }

                const plane p{drag.dragPlaneNormal, -dot(drag.dragPlaneNormal, drag.startPivot)};
                vec3 currentPoint;

                if (gizmo::ray_plane_intersection(get_mouse_ray(frame), p, currentPoint))
                {
                    const vec3 delta = currentPoint - drag.startGrabPoint;
                    const vec3 worldDelta = is_axis(drag.handle) ? drag.axisDir * dot(delta, drag.axisDir) : delta;

                    const quaternion toLocal = drag.startLocalRotation * inverse(drag.startRotation);

                    out.position = drag.startLocalPosition + transform(toLocal, worldDelta);
                }
                else
                {
                    out.position = drag.startLocalPosition;
                }

                return true;
            }

            hovered = pick_translation_handle(frame);

            if (hovered != gizmo_handle_type::none && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (is_axis(hovered))
                {
                    const vec3 planeNormal = normalize(frame.cameraPosition - frame.pivot);

                    begin_drag(drag,
                        hovered,
                        isJoint,
                        jointIndex,
                        frame,
                        world,
                        local,
                        planeNormal,
                        AxisDirections[axis_index(hovered)],
                        {},
                        0.f);
                }
                else
                {
                    u32 a;
                    u32 b;
                    handle_axes(hovered, a, b);

                    const vec3 planeNormal = normalize(cross(AxisDirections[a], AxisDirections[b]));

                    begin_drag(drag,
                        hovered,
                        isJoint,
                        jointIndex,
                        frame,
                        world,
                        local,
                        planeNormal,
                        AxisDirections[a],
                        AxisDirections[b],
                        0.f);
                }

                out.position = local.position;
                return true;
            }

            return hovered != gizmo_handle_type::none;
        }

        // Runs the rotation gizmo interaction. See manipulate_translation.
        bool manipulate_rotation(gizmo_drag_state& drag,
            gizmo_handle_type& hovered,
            const gizmo_frame& frame,
            bool isJoint,
            u32 jointIndex,
            const trs& world,
            const trs& local,
            trs& out)
        {
            out.position = local.position;
            out.scale = local.scale;

            if (drag.active)
            {
                if (drag.isJoint != isJoint || drag.jointIndex != jointIndex)
                {
                    return false;
                }

                const plane p{drag.axisDir, -dot(drag.axisDir, drag.startPivot)};
                vec3 currentPoint;

                if (gizmo::ray_plane_intersection(get_mouse_ray(frame), p, currentPoint))
                {
                    const f32 currentAngle = std::atan2(dot(currentPoint - drag.startPivot, drag.basisV),
                        dot(currentPoint - drag.startPivot, drag.basisU));

                    const f32 delta = currentAngle - drag.startAngle;
                    const quaternion newWorldRotation =
                        quaternion::from_axis_angle(drag.axisDir, radians{delta}) * drag.startRotation;

                    out.rotation = normalize(drag.startLocalRotation * inverse(drag.startRotation) * newWorldRotation);
                }
                else
                {
                    out.rotation = drag.startLocalRotation;
                }

                return true;
            }

            hovered = pick_rotation_handle(frame);

            if (hovered != gizmo_handle_type::none && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const vec3 axis = hovered == gizmo_handle_type::ring_view
                    ? normalize(frame.cameraPosition - frame.pivot)
                    : AxisDirections[ring_axis_index(hovered)];

                vec3 u = normalize(cross(axis, vec3{.y = 1.f}));

                if (length2(u) < 1e-6f)
                {
                    u = normalize(cross(axis, vec3{.x = 1.f}));
                }

                const vec3 v = normalize(cross(axis, u));

                begin_drag(drag, hovered, isJoint, jointIndex, frame, world, local, axis, axis, {}, 0.f);

                drag.basisU = u;
                drag.basisV = v;

                const plane p{axis, -dot(axis, frame.pivot)};

                if (gizmo::ray_plane_intersection(get_mouse_ray(frame), p, drag.startGrabPoint))
                {
                    drag.startAngle = std::atan2(dot(drag.startGrabPoint - drag.startPivot, v),
                        dot(drag.startGrabPoint - drag.startPivot, u));
                }
                else
                {
                    drag.startAngle = 0.f;
                }

                out.rotation = local.rotation;
                return true;
            }

            return hovered != gizmo_handle_type::none;
        }

        // Runs the scale gizmo interaction. See manipulate_translation.
        bool manipulate_scale(gizmo_drag_state& drag,
            gizmo_handle_type& hovered,
            const gizmo_frame& frame,
            bool isJoint,
            u32 jointIndex,
            const trs& world,
            const trs& local,
            trs& out)
        {
            out.position = local.position;
            out.rotation = local.rotation;

            if (drag.active)
            {
                if (drag.isJoint != isJoint || drag.jointIndex != jointIndex)
                {
                    return false;
                }

                const plane p{drag.dragPlaneNormal, -dot(drag.dragPlaneNormal, drag.startPivot)};
                vec3 currentPoint;

                if (gizmo::ray_plane_intersection(get_mouse_ray(frame), p, currentPoint))
                {
                    const vec3 delta = currentPoint - drag.startGrabPoint;
                    const f32 factor = max(0.01f, 1.f + dot(delta, drag.axisDir) / drag.referenceLength);

                    if (drag.handle == gizmo_handle_type::center)
                    {
                        out.scale = drag.startLocalScale * factor;
                    }
                    else
                    {
                        out.scale = drag.startLocalScale;
                        out.scale[axis_index(drag.handle)] *= factor;
                    }
                }
                else
                {
                    out.scale = drag.startLocalScale;
                }

                return true;
            }

            hovered = pick_scale_handle(frame);

            if (hovered != gizmo_handle_type::none && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const vec3 planeNormal = normalize(frame.cameraPosition - frame.pivot);
                const vec3 axisDir =
                    hovered == gizmo_handle_type::center ? planeNormal : AxisDirections[axis_index(hovered)];

                begin_drag(drag,
                    hovered,
                    isJoint,
                    jointIndex,
                    frame,
                    world,
                    local,
                    planeNormal,
                    axisDir,
                    {},
                    frame.pixelsToWorld * AxisLengthPixels);

                out.scale = local.scale;
                return true;
            }

            return hovered != gizmo_handle_type::none;
        }

        void draw_translation(const gizmo_frame& frame, gizmo_handle_type active, gizmo_handle_type hovered)
        {
            const f32 planeSizeWorld = plane_size(frame);

            for (u32 i = 0; i < AxisCount; ++i)
            {
                const u32 j = (i + 1) % AxisCount;

                gizmo_handle_type handle;
                switch (i)
                {
                case 0:
                    handle = gizmo_handle_type::plane_xy;
                    break;
                case 1:
                    handle = gizmo_handle_type::plane_yz;
                    break;
                default:
                    handle = gizmo_handle_type::plane_xz;
                    break;
                }

                const vec2 corners[4] = {
                    project_point(frame, frame.pivot),
                    project_point(frame, frame.pivot + AxisDirections[i] * planeSizeWorld),
                    project_point(frame, frame.pivot + (AxisDirections[i] + AxisDirections[j]) * planeSizeWorld),
                    project_point(frame, frame.pivot + AxisDirections[j] * planeSizeWorld),
                };

                const bool highlighted = active == handle || hovered == handle;

                const ImU32 color = highlighted ? with_alpha(ActiveColor, PlaneAlpha)
                                                : with_alpha(blend_colors(axis_color(i), axis_color(j)), PlaneAlpha);

                gizmo::draw_quad(frame.drawList, corners, color);
            }

            for (u32 i = 0; i < AxisCount; ++i)
            {
                const auto handle = gizmo_handle_type(u32(gizmo_handle_type::axis_x) + i);
                const bool highlighted = active == handle || hovered == handle;
                const ImU32 color = highlighted ? (active == handle ? ActiveColor : HoverColor) : axis_color(i);

                gizmo::draw_arrow(frame.drawList,
                    project_point(frame, frame.pivot),
                    project_point(frame, axis_end(frame, i)),
                    color,
                    AxisThickness,
                    ArrowHeadSize);
            }
        }

        void draw_rotation(const gizmo_frame& frame, gizmo_handle_type active, gizmo_handle_type hovered)
        {
            const f32 radius = frame.pixelsToWorld * RingRadiusPixels;

            for (u32 i = 0; i < AxisCount; ++i)
            {
                const auto handle = gizmo_handle_type(u32(gizmo_handle_type::ring_x) + i);
                const bool highlighted = active == handle || hovered == handle;
                const ImU32 color = highlighted ? (active == handle ? ActiveColor : HoverColor) : axis_color(i);

                gizmo::draw_ring(frame.drawList,
                    frame.viewProj,
                    frame.origin,
                    frame.size,
                    frame.pivot,
                    AxisDirections[i],
                    radius,
                    color,
                    RingThickness,
                    RingSegments);
            }

            const vec3 viewDir = normalize(frame.cameraPosition - frame.pivot);
            const bool highlighted = active == gizmo_handle_type::ring_view || hovered == gizmo_handle_type::ring_view;
            const ImU32 color =
                highlighted ? (active == gizmo_handle_type::ring_view ? ActiveColor : HoverColor) : ViewRingColor;

            gizmo::draw_ring(frame.drawList,
                frame.viewProj,
                frame.origin,
                frame.size,
                frame.pivot,
                viewDir,
                radius,
                color,
                RingThickness,
                RingSegments);
        }

        void draw_scale(const gizmo_frame& frame, gizmo_handle_type active, gizmo_handle_type hovered)
        {
            for (u32 i = 0; i < AxisCount; ++i)
            {
                const auto handle = gizmo_handle_type(u32(gizmo_handle_type::axis_x) + i);
                const bool highlighted = active == handle || hovered == handle;
                const ImU32 color = highlighted ? (active == handle ? ActiveColor : HoverColor) : axis_color(i);

                gizmo::draw_arrow(frame.drawList,
                    project_point(frame, frame.pivot),
                    project_point(frame, axis_end(frame, i)),
                    color,
                    AxisThickness,
                    ArrowHeadSize * .5f);

                const vec2 tip = project_point(frame, axis_end(frame, i));
                frame.drawList->AddRectFilled({tip.x - ScaleBoxSizePixels, tip.y - ScaleBoxSizePixels},
                    {tip.x + ScaleBoxSizePixels, tip.y + ScaleBoxSizePixels},
                    color);
            }

            const vec2 center = project_point(frame, frame.pivot);
            const bool highlighted = active == gizmo_handle_type::center || hovered == gizmo_handle_type::center;
            const ImU32 color = highlighted ? (active == gizmo_handle_type::center ? ActiveColor : HoverColor)
                                            : IM_COL32(120, 120, 120, 220);

            frame.drawList->AddRectFilled({center.x - ScaleBoxSizePixels, center.y - ScaleBoxSizePixels},
                {center.x + ScaleBoxSizePixels, center.y + ScaleBoxSizePixels},
                color);
        }

        void draw_gizmo(
            gizmo_handler::operation op, const gizmo_frame& frame, gizmo_handle_type active, gizmo_handle_type hovered)
        {
            switch (op)
            {
            case gizmo_handler::operation::translation:
                draw_translation(frame, active, hovered);
                break;

            case gizmo_handler::operation::rotation:
                draw_rotation(frame, active, hovered);
                break;

            case gizmo_handler::operation::scale:
                draw_scale(frame, active, hovered);
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

    struct gizmo_handler::impl
    {
        u32 id{};
        operation op{};
        bool surfaceSnapping{};
        gizmo_drag_state drag{};
        gizmo_handle_type hovered{};

        void update_surface_snap(ecs::entity_registry& reg,
            viewport_component* viewport,
            ecs::entity e,
            vec2 pickingCoordinates,
            vec3& outPosition);
    };

    gizmo_handler::gizmo_handler() = default;

    gizmo_handler::~gizmo_handler() = default;

    void gizmo_handler::init()
    {
        m_impl = allocate_unique<impl>();
    }

    void gizmo_handler::set_id(u32 id)
    {
        m_impl->id = id;
    }

    gizmo_handler::operation gizmo_handler::get_operation() const
    {
        return m_impl->op;
    }

    void gizmo_handler::set_operation(operation op)
    {
        m_impl->op = op;
    }

    void gizmo_handler::impl::update_surface_snap(ecs::entity_registry& reg,
        viewport_component* viewport,
        ecs::entity e,
        vec2 pickingCoordinates,
        vec3& outPosition)
    {
        if (!viewport || !drag.active || drag.isJoint)
        {
            return;
        }

        const bool wantSnap = op == operation::translation;

        switch (viewport->picking.state)
        {
        case picking_request::state::none:
            if (wantSnap)
            {
                viewport->picking.coordinates = pickingCoordinates;
                viewport->picking.state = picking_request::state::requested;
                surfaceSnapping = true;
            }
            break;

        case picking_request::state::served: {
            const auto& result = viewport->picking.result;
            const ecs::entity pickedEntity{result.entityId};

            if (pickedEntity && pickedEntity != e && reg.contains(pickedEntity))
            {
                const vec3 surfacePosition{result.position.x, result.position.y, result.position.z};
                const vec3 surfaceNormal{result.normal.x, result.normal.y, result.normal.z};

                outPosition = surfacePosition + surfaceNormal * SurfaceSnapOffset;
            }

            if (wantSnap)
            {
                viewport->picking.coordinates = pickingCoordinates;
                viewport->picking.state = picking_request::state::requested;
            }
            else
            {
                surfaceSnapping = false;
                viewport->picking.state = picking_request::state::none;
            }
            break;
        }

        case picking_request::state::failed:
            surfaceSnapping = false;
            viewport->picking.state = picking_request::state::none;
            break;

        case picking_request::state::awaiting:
            break;

        default:
            unreachable();
        }
    }

    bool gizmo_handler::handle(const resource_registry& resources,
        ecs::entity_registry& reg,
        std::span<const ecs::entity> entities,
        vec2 origin,
        vec2 size,
        const ecs::entity cameraEntity,
        viewport_component* viewport)
    {
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

        const auto [view, projection] = [&reg, cameraEntity, size]() -> pair<mat4, mat4>
        {
            const auto& [camera, cameraWorld] = reg.get<camera_component, global_transform_component>(cameraEntity);

            const mat4 view = inverse(cameraWorld.localToWorld).assert_value_or(mat4::identity());

            const f32 ratio = f32(size.y) / size.x;

            mat4 projection = make_perspective_matrix(camera.fovy, ratio, camera.near, camera.far);

            // Flip Y to change handedness
            projection.at(1, 1) = -projection.at(1, 1);

            return {view, projection};
        }();

        const mat4 viewProj = projection * view;
        const mat4 invViewProj = inverse(viewProj).assert_value_or(mat4::identity());

        const auto& cameraWorld = reg.get<global_transform_component>(cameraEntity);

        const vec3 cameraPosition = {
            cameraWorld.localToWorld.columns[3].x,
            cameraWorld.localToWorld.columns[3].y,
            cameraWorld.localToWorld.columns[3].z,
        };

        const vec3 cameraForward = {
            cameraWorld.localToWorld.columns[2].x,
            cameraWorld.localToWorld.columns[2].y,
            cameraWorld.localToWorld.columns[2].z,
        };

        const vec2 mouse = {ImGui::GetMousePos().x, ImGui::GetMousePos().y};
        ImDrawList* const drawList = ImGui::GetWindowDrawList();

        m_impl->hovered = gizmo_handle_type::none;

        bool interacting = false;

        if (reg.has<joint_pose_component, joint_skinning_transform_component>(e))
        {
            if (m_impl->drag.active && !m_impl->drag.isJoint)
            {
                end_drag(m_impl->drag, m_impl->surfaceSnapping, viewport);
            }

            auto&& [poseComp, jointTransforms] = reg.get<joint_pose_component, joint_skinning_transform_component>(e);

            static_assert(
                joint_pose_component::joints_per_chunk == joint_skinning_transform_component::joints_per_chunk);

            for (u32 jointIndex = 0; jointIndex < joint_pose_component::joints_per_chunk; ++jointIndex)
            {
                mat4 globalJointTransform = jointTransforms.jointMatrices[jointIndex] *
                    inverse(poseComp.invBindPoses[jointIndex]).value_or(mat4::identity());

                vec3 worldPosition;
                quaternion worldRotation;
                vec3 worldScale;

                if (!decompose_matrix(globalJointTransform, worldPosition, worldRotation, worldScale))
                {
                    continue;
                }

                const auto& localPose = poseComp.localPoses[jointIndex];

                const trs world{worldPosition, worldRotation, worldScale};
                const trs local{localPose.translation, localPose.rotation, localPose.scale};

                const vec3 pivot = worldPosition;

                const gizmo_frame frame{
                    .origin = origin,
                    .size = size,
                    .viewProj = viewProj,
                    .invViewProj = invViewProj,
                    .cameraPosition = cameraPosition,
                    .cameraForward = cameraForward,
                    .pivot = pivot,
                    .pixelsToWorld = gizmo::compute_gizmo_scale(viewProj, pivot, size.y, 1.f),
                    .mouse = mouse,
                    .drawList = drawList,
                };

                trs out;

                switch (m_impl->op)
                {
                case operation::translation:
                    interacting |= manipulate_translation(m_impl->drag,
                        m_impl->hovered,
                        frame,
                        true,
                        jointIndex,
                        world,
                        local,
                        out);
                    break;

                case operation::rotation:
                    interacting |=
                        manipulate_rotation(m_impl->drag, m_impl->hovered, frame, true, jointIndex, world, local, out);
                    break;

                case operation::scale:
                    interacting |=
                        manipulate_scale(m_impl->drag, m_impl->hovered, frame, true, jointIndex, world, local, out);
                    break;

                default:
                    unreachable();
                }

                const gizmo_handle_type active =
                    m_impl->drag.active && m_impl->drag.isJoint && m_impl->drag.jointIndex == jointIndex
                    ? m_impl->drag.handle
                    : gizmo_handle_type::none;

                draw_gizmo(m_impl->op, frame, active, m_impl->hovered);

                if (m_impl->drag.active && m_impl->drag.isJoint && m_impl->drag.jointIndex == jointIndex)
                {
                    poseComp.localPoses[jointIndex].translation = out.position;
                    poseComp.localPoses[jointIndex].rotation = out.rotation;
                    poseComp.localPoses[jointIndex].scale = out.scale;
                    reg.notify(e);
                }
            }

            if (auto* const parent = reg.try_get<parent_component>(e))
            {
                draw_joints(resources, reg, parent->parent, projection * view);
            }
        }

        if (reg.has<joint_skinning_transform_chunks_component>(e))
        {
            draw_joints(resources, reg, e, projection * view);
        }

        if (!interacting &&
            reg.has<position_component, rotation_component, scale_component, global_transform_component>(e))
        {
            if (m_impl->drag.active && m_impl->drag.isJoint)
            {
                end_drag(m_impl->drag, m_impl->surfaceSnapping, viewport);
            }

            auto&& [positionComp, rotationComp, scaleComp, transformComp] =
                reg.get<position_component, rotation_component, scale_component, global_transform_component>(e);

            vec3 worldPosition;
            quaternion worldRotation;
            vec3 worldScale;

            if (!decompose_matrix(transformComp.localToWorld, worldPosition, worldRotation, worldScale))
            {
                worldPosition = {
                    transformComp.localToWorld.columns[3].x,
                    transformComp.localToWorld.columns[3].y,
                    transformComp.localToWorld.columns[3].z,
                };
                worldRotation = rotationComp.value;
                worldScale = scaleComp.value;
            }

            const trs world{worldPosition, worldRotation, worldScale};
            const trs local{positionComp.value, rotationComp.value, scaleComp.value};

            const vec3 pivot = worldPosition;

            const gizmo_frame frame{
                .origin = origin,
                .size = size,
                .viewProj = viewProj,
                .invViewProj = invViewProj,
                .cameraPosition = cameraPosition,
                .cameraForward = cameraForward,
                .pivot = pivot,
                .pixelsToWorld = gizmo::compute_gizmo_scale(viewProj, pivot, size.y, 1.f),
                .mouse = mouse,
                .drawList = drawList,
            };

            trs out = local;

            switch (m_impl->op)
            {
            case operation::translation:
                interacting |=
                    manipulate_translation(m_impl->drag, m_impl->hovered, frame, false, 0, world, local, out);
                break;

            case operation::rotation:
                interacting |= manipulate_rotation(m_impl->drag, m_impl->hovered, frame, false, 0, world, local, out);
                break;

            case operation::scale:
                interacting |= manipulate_scale(m_impl->drag, m_impl->hovered, frame, false, 0, world, local, out);
                break;

            default:
                unreachable();
            }

            if (m_impl->op == operation::translation)
            {
                m_impl->update_surface_snap(reg, viewport, e, mouse - origin, out.position);
            }

            const gizmo_handle_type active =
                m_impl->drag.active && !m_impl->drag.isJoint ? m_impl->drag.handle : gizmo_handle_type::none;

            draw_gizmo(m_impl->op, frame, active, m_impl->hovered);

            if (m_impl->drag.active && !m_impl->drag.isJoint)
            {
                positionComp.value = out.position;
                rotationComp.value = out.rotation;
                scaleComp.value = out.scale;
                reg.notify(e);
            }
        }

        if (m_impl->drag.active && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            end_drag(m_impl->drag, m_impl->surfaceSnapping, viewport);
        }

        return interacting || m_impl->drag.active;
    }
}
