#include <oblo/editor/utility/gizmo.hpp>

#include <oblo/core/utility.hpp>
#include <oblo/math/constants.hpp>
#include <oblo/math/plane.hpp>
#include <oblo/math/vec4.hpp>

#include <imgui.h>

#include <cmath>

namespace oblo::editor::gizmo
{
    vec2 world_to_screen(const mat4& viewProj, vec2 origin, vec2 size, const vec3& p)
    {
        const vec4 projected = viewProj * vec4{p.x, p.y, p.z, 1.f};
        const vec2 ndc = vec2{projected.x, projected.y} / projected.w;

        return {
            origin.x + (ndc.x + 1.f) * .5f * size.x,
            origin.y + (1.f - ndc.y) * .5f * size.y,
        };
    }

    ray screen_to_world_ray(const mat4& invViewProj, vec2 origin, vec2 size, vec2 screen)
    {
        const vec2 ndc = {
            (screen.x - origin.x) / size.x * 2.f - 1.f,
            1.f - (screen.y - origin.y) / size.y * 2.f,
        };

        const auto unproject = [&invViewProj, &ndc](f32 z) -> vec3
        {
            const vec4 world = invViewProj * vec4{ndc.x, ndc.y, z, 1.f};
            return vec3{world.x, world.y, world.z} / world.w;
        };

        // The projection matrix uses reverse-Z, so near maps to clip z = 1 and far to clip z = 0.
        const vec3 near = unproject(1.f);
        const vec3 far = unproject(0.f);

        return ray{.origin = near, .direction = normalize(far - near)};
    }

    bool ray_plane_intersection(const ray& r, const plane& p, vec3& outPoint)
    {
        const f32 denom = dot(p.normal, r.direction);

        if (std::abs(denom) < 1e-6f)
        {
            return false;
        }

        const f32 t = -(dot(p.normal, r.origin) + p.offset) / denom;

        if (t < 0.f)
        {
            return false;
        }

        outPoint = r.origin + r.direction * t;
        return true;
    }

    f32 distance_to_segment(const vec2& p, const vec2& a, const vec2& b)
    {
        const vec2 ab = b - a;
        const f32 len2 = length2(ab);

        if (len2 < 1e-8f)
        {
            return length(p - a);
        }

        const f32 t = max(0.f, min(1.f, dot(p - a, ab) / len2));
        return length(p - (a + ab * t));
    }

    bool point_in_quad(const vec2& p, const vec2 (&corners)[4])
    {
        f32 sign = 0.f;

        for (u32 i = 0; i < 4; ++i)
        {
            const vec2& a = corners[i];
            const vec2& b = corners[(i + 1) % 4];

            const f32 cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);

            if (cross != 0.f)
            {
                if (sign == 0.f)
                {
                    sign = cross;
                }
                else if ((sign > 0.f) != (cross > 0.f))
                {
                    return false;
                }
            }
        }

        return true;
    }

    f32 compute_gizmo_scale(const mat4& viewProj, const vec3& pivot, f32 viewportHeight, f32 desiredPixels)
    {
        const vec4 clip = viewProj * vec4{pivot.x, pivot.y, pivot.z, 1.f};

        if (clip.w <= 0.f)
        {
            return 1.f;
        }

        // clip.w is the view-space depth, and at(1, 1) is the vertical focal length. Together they give the world
        // size covered by a single pixel at the pivot's depth.
        const f32 worldPerPixel = (2.f * clip.w / viewProj.at(1, 1)) / max(1.f, viewportHeight);

        return desiredPixels * worldPerPixel;
    }

    void draw_arrow(ImDrawList* drawList, vec2 from, vec2 to, ImU32 color, f32 thickness, f32 headSize)
    {
        drawList->AddLine({from.x, from.y}, {to.x, to.y}, color, thickness);

        const vec2 dir = to - from;
        const f32 len = length(dir);

        if (len < headSize * 2.f)
        {
            return;
        }

        const vec2 n = dir / len;
        const vec2 p = vec2{-n.y, n.x};
        const vec2 tip = to;
        const vec2 base = to - n * headSize;
        const vec2 halfWidth = p * headSize * .5f;

        drawList->AddTriangleFilled({tip.x, tip.y},
            {(base + halfWidth).x, (base + halfWidth).y},
            {(base - halfWidth).x, (base - halfWidth).y},
            color);
    }

    void draw_ring(ImDrawList* drawList,
        const mat4& viewProj,
        vec2 origin,
        vec2 size,
        const vec3& center,
        const vec3& normal,
        f32 radius,
        ImU32 color,
        f32 thickness,
        u32 segments)
    {
        constexpr u32 MaxSegments = 64;
        segments = min<u32>(segments, MaxSegments);

        vec3 u = normalize(cross(normal, vec3{.y = 1.f}));

        if (length2(u) < 1e-6f)
        {
            u = normalize(cross(normal, vec3{.x = 1.f}));
        }

        const vec3 v = normalize(cross(normal, u));

        ImVec2 points[MaxSegments];

        for (u32 i = 0; i < segments; ++i)
        {
            const f32 angle = f32(i) / f32(segments) * 2.f * pi;
            const vec3 pointOnRing = center + u * (std::cos(angle) * radius) + v * (std::sin(angle) * radius);
            const vec2 screen = world_to_screen(viewProj, origin, size, pointOnRing);
            points[i] = {screen.x, screen.y};
        }

        drawList->AddPolyline(points, int(segments), color, ImDrawFlags_Closed, thickness);
    }

    void draw_quad(ImDrawList* drawList, const vec2 (&corners)[4], ImU32 color)
    {
        drawList->AddQuadFilled({corners[0].x, corners[0].y},
            {corners[1].x, corners[1].y},
            {corners[2].x, corners[2].y},
            {corners[3].x, corners[3].y},
            color);
    }
}
