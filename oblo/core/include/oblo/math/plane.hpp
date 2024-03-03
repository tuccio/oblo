#pragma once

#include <oblo/math/vec3.hpp>

namespace oblo
{
    // Represent a plane equation in the form: dot(normal, v) + offset = 0
    struct plane
    {
        vec3 normal;
        f32 offset;
    };

    inline plane normalize(const plane& p)
    {
        const f32 norm = length(p.normal);
        return {.normal = p.normal / norm, .offset = p.offset / norm};
    }

    inline plane make_plane(const vec3& p1, const vec3& p2, const vec3& p3)
    {
        const vec3 v12 = p2 - p1;
        const vec3 v13 = p3 - p1;

        const auto normal = normalize(cross(v12, v13));

        return {.normal = normal, .offset = -dot(normal, p1)};
    }
}