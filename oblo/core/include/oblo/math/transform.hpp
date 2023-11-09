#pragma once

#include <oblo/math/mat4.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    constexpr mat4 make_transform_matrix(const vec3& p, const quaternion& r, const vec3&)
    {
        const f32 rx2 = r.x * r.x;
        const f32 ry2 = r.y * r.y;
        const f32 rz2 = r.z * r.z;

        return {{
            {
                1 - 2 * ry2 - 2 * rz2,
                2 * r.x * r.y - 2 * r.z * r.w,
                2 * r.x * r.z + 2 * r.y * r.w,
                p.x,
            },
            {
                2 * r.x * r.y + 2 * r.z * r.w,
                1 - 2 * rx2 - 2 * rz2,
                2 * r.y * r.z - 2 * r.x * r.w,
                p.y,
            },
            {
                2 * r.x * r.z - 2 * r.y * r.w,
                2 * r.y * r.z + 2 * r.x * r.w,
                1 - 2 * rx2 - 2 * ry2,
                p.z,
            },
            {
                0.f,
                0.f,
                0.f,
                1.f,
            },
        }};
    }
}