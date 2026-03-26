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

    inline expected<> decompose_matrix(const mat4& m, vec3& t, quaternion& r, vec3& s)
    {
        // 1. Extract Translation from the 4th column (index 3)
        t = vec3{m.columns[3].x, m.columns[3].y, m.columns[3].z};

        // Each scale component is the length of the corresponding basis vector
        vec3 col0 = vec3{m.columns[0].x, m.columns[0].y, m.columns[0].z};
        const vec3 col1 = vec3{m.columns[1].x, m.columns[1].y, m.columns[1].z};
        const vec3 col2 = vec3{m.columns[2].x, m.columns[2].y, m.columns[2].z};

        s.x = length(col0);
        s.y = length(col1);
        s.z = length(col2);

        // If the determinant is negative, the model is mirrored.
        // We flip one scale axis to preserve the handedness for rotation extraction.
        const f32 det = (col0.x * (col1.y * col2.z - col1.z * col2.y) - col0.y * (col1.x * col2.z - col1.z * col2.x) +
            col0.z * (col1.x * col2.y - col1.y * col2.x));

        if (det < 0)
        {
            s.x = -s.x;
            col0 = -col0;
        }

        if (std::abs(s.x) < 1e-6f || std::abs(s.y) < 1e-6f || std::abs(s.z) < 1e-6f)
        {
            return "Can't decompose singular matrix"_err;
        }

        // Remove the scale to calculate the rotation
        const f32 rsx = 1.f / s.x;
        const f32 rsy = 1.f / s.y;
        const f32 rsz = 1.f / s.z;

        const f32 m00 = col0.x * rsx;
        const f32 m01 = col1.x * rsy;
        const f32 m02 = col2.x * rsz;
        const f32 m10 = col0.y * rsx;
        const f32 m11 = col1.y * rsy;
        const f32 m12 = col2.y * rsz;
        const f32 m20 = col0.z * rsx;
        const f32 m21 = col1.z * rsy;
        const f32 m22 = col2.z * rsz;

        // Effectively convert the mat3 to a quaternion
        const f32 tr = m00 + m11 + m22;

        if (tr > 0)
        {
            const f32 S = std::sqrtf(tr + 1.0f) * 2.0f;
            r.w = 0.25f * S;
            r.x = (m21 - m12) / S;
            r.y = (m02 - m20) / S;
            r.z = (m10 - m01) / S;
        }
        else if ((m00 > m11) && (m00 > m22))
        {
            const f32 S = std::sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
            r.w = (m21 - m12) / S;
            r.x = 0.25f * S;
            r.y = (m01 + m10) / S;
            r.z = (m02 + m20) / S;
        }
        else if (m11 > m22)
        {
            const f32 S = std::sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
            r.w = (m02 - m20) / S;
            r.x = (m01 + m10) / S;
            r.y = 0.25f * S;
            r.z = (m12 + m21) / S;
        }
        else
        {
            const f32 S = std::sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
            r.w = (m10 - m01) / S;
            r.x = (m02 + m20) / S;
            r.y = (m12 + m21) / S;
            r.z = 0.25f * S;
        }

        return no_error;
    }
}
