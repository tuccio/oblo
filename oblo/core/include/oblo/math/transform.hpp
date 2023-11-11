#pragma once

#include <oblo/math/mat4.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    constexpr mat4 make_transform_matrix(const vec3& p, const quaternion& r, const vec3& s)
    {
        const f32 rx2 = r.x * r.x;
        const f32 ry2 = r.y * r.y;
        const f32 rz2 = r.z * r.z;

        return {{
            vec4{
                s.x * (1 - 2 * ry2 - 2 * rz2),
                s.y * (2 * r.x * r.y - 2 * r.z * r.w),
                s.z * (2 * r.x * r.z + 2 * r.y * r.w),
                p.x,
            },
            vec4{
                s.x * (2 * r.x * r.y + 2 * r.z * r.w),
                s.y * (1 - 2 * rx2 - 2 * rz2),
                s.z * (2 * r.y * r.z - 2 * r.x * r.w),
                p.y,
            },
            vec4{
                s.x * (2 * r.x * r.z - 2 * r.y * r.w),
                s.y * (2 * r.y * r.z + 2 * r.x * r.w),
                s.z * (1 - 2 * rx2 - 2 * ry2),
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