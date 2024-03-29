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
                s.x * (2 * r.x * r.y + 2 * r.z * r.w),
                s.x * (2 * r.x * r.z - 2 * r.y * r.w),
                0.f,
            },
            vec4{
                s.y * (2 * r.x * r.y - 2 * r.z * r.w),
                s.y * (1 - 2 * rx2 - 2 * rz2),
                s.y * (2 * r.y * r.z + 2 * r.x * r.w),
                0.f,
            },
            vec4{
                s.z * (2 * r.x * r.z + 2 * r.y * r.w),
                s.z * (2 * r.y * r.z - 2 * r.x * r.w),
                s.z * (1 - 2 * rx2 - 2 * ry2),
                0.f,
            },
            {
                p.x,
                p.y,
                p.z,
                1.f,
            },
        }};
    }
}