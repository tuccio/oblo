#pragma once

#include <oblo/math/mat4.hpp>
#include <oblo/math/ray.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>

#include <imgui.h>

struct ImDrawList;

namespace oblo
{
    struct plane;
}

namespace oblo::editor
{
    namespace gizmo
    {
        // Converts a world position to screen coordinates, given the view-projection matrix and the viewport rect.
        vec2 world_to_screen(const mat4& viewProj, vec2 origin, vec2 size, const vec3& p);

        // Builds a world-space ray from the given screen coordinates. The engine projection matrix uses reverse-Z,
        // so near clip z is 1 and far clip z is 0.
        ray screen_to_world_ray(const mat4& invViewProj, vec2 origin, vec2 size, vec2 screen);

        // Intersects a ray with a plane, filling outPoint if it hits.
        bool ray_plane_intersection(const ray& r, const plane& p, vec3& outPoint);

        // Screen-space distance from point p to the segment [a, b].
        f32 distance_to_segment(const vec2& p, const vec2& a, const vec2& b);

        // Whether point p is inside the convex quad defined by the 4 corners (in winding order).
        bool point_in_quad(const vec2& p, const vec2 (&corners)[4]);

        // World-space length that projects to roughly desiredPixels on screen at the pivot's depth.
        f32 compute_gizmo_scale(const mat4& viewProj, const vec3& pivot, f32 viewportHeight, f32 desiredPixels);

        void draw_arrow(ImDrawList* drawList, vec2 from, vec2 to, ImU32 color, f32 thickness, f32 headSize);

        void draw_ring(ImDrawList* drawList,
            const mat4& viewProj,
            vec2 origin,
            vec2 size,
            const vec3& center,
            const vec3& normal,
            f32 radius,
            ImU32 color,
            f32 thickness,
            u32 segments = 48);

        void draw_quad(ImDrawList* drawList, const vec2 (&corners)[4], ImU32 color);
    }
}
