#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/types.hpp>
#include <oblo/math/constants.hpp>
#include <oblo/math/vec4.hpp>

namespace oblo
{
    struct mat4
    {
        vec4 columns[4];

        constexpr const f32& at(u32 i, u32 j) const
        {
            return columns[j][i];
        }

        constexpr f32& at(u32 i, u32 j)
        {
            return columns[j][i];
        }

        constexpr static mat4 identity()
        {
            return mat4{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
        }
    };

    constexpr mat4 operator*(const mat4& lhs, const mat4& rhs)
    {
        mat4 r;

        for (u32 i = 0; i < 4; ++i)
        {
            for (u32 j = 0; j < 4; ++j)
            {
                float d = 0;

                for (u32 k = 0; k < 4; ++k)
                {
                    d += lhs.at(i, k) * rhs.at(k, j);
                }

                r.at(i, j) = d;
            }
        }

        return r;
    }

    constexpr vec4 operator*(const mat4& lhs, const vec4& rhs)
    {
        vec4 r;

        for (u32 i = 0; i < 4; ++i)
        {
            r[i] = lhs.columns[0][i] * rhs[0] + lhs.columns[1][i] * rhs[1] + lhs.columns[2][i] * rhs[2] +
                lhs.columns[3][i] * rhs[3];
        }

        return r;
    }

    constexpr mat4 operator*(const mat4& lhs, const f32 rhs)
    {
        mat4 r;

        for (u32 i = 0; i < 4; ++i)
        {
            r.columns[i] = lhs.columns[i] * rhs;
        }

        return r;
    }

    constexpr mat4 operator*(const f32 lhs, const mat4& rhs)
    {
        return rhs * lhs;
    }

    constexpr mat4 transpose(const mat4& m)
    {
        mat4 r;

        for (u32 i = 0; i < 4; ++i)
        {
            for (u32 j = 0; j < 4; ++j)
            {
                r.at(i, j) = m.at(j, i);
            }
        }

        return r;
    }

    constexpr expected<mat4> inverse(const mat4& m, f32* outDeterminant = nullptr)
    {
        mat4 inv;

        inv.at(0, 0) = m.at(1, 1) * m.at(2, 2) * m.at(3, 3) - m.at(1, 1) * m.at(2, 3) * m.at(3, 2) -
            m.at(2, 1) * m.at(1, 2) * m.at(3, 3) + m.at(2, 1) * m.at(1, 3) * m.at(3, 2) +
            m.at(3, 1) * m.at(1, 2) * m.at(2, 3) - m.at(3, 1) * m.at(1, 3) * m.at(2, 2);

        inv.at(1, 0) = -m.at(1, 0) * m.at(2, 2) * m.at(3, 3) + m.at(1, 0) * m.at(2, 3) * m.at(3, 2) +
            m.at(2, 0) * m.at(1, 2) * m.at(3, 3) - m.at(2, 0) * m.at(1, 3) * m.at(3, 2) -
            m.at(3, 0) * m.at(1, 2) * m.at(2, 3) + m.at(3, 0) * m.at(1, 3) * m.at(2, 2);

        inv.at(2, 0) = m.at(1, 0) * m.at(2, 1) * m.at(3, 3) - m.at(1, 0) * m.at(2, 3) * m.at(3, 1) -
            m.at(2, 0) * m.at(1, 1) * m.at(3, 3) + m.at(2, 0) * m.at(1, 3) * m.at(3, 1) +
            m.at(3, 0) * m.at(1, 1) * m.at(2, 3) - m.at(3, 0) * m.at(1, 3) * m.at(2, 1);

        inv.at(3, 0) = -m.at(1, 0) * m.at(2, 1) * m.at(3, 2) + m.at(1, 0) * m.at(2, 2) * m.at(3, 1) +
            m.at(2, 0) * m.at(1, 1) * m.at(3, 2) - m.at(2, 0) * m.at(1, 2) * m.at(3, 1) -
            m.at(3, 0) * m.at(1, 1) * m.at(2, 2) + m.at(3, 0) * m.at(1, 2) * m.at(2, 1);

        inv.at(0, 1) = -m.at(0, 1) * m.at(2, 2) * m.at(3, 3) + m.at(0, 1) * m.at(2, 3) * m.at(3, 2) +
            m.at(2, 1) * m.at(0, 2) * m.at(3, 3) - m.at(2, 1) * m.at(0, 3) * m.at(3, 2) -
            m.at(3, 1) * m.at(0, 2) * m.at(2, 3) + m.at(3, 1) * m.at(0, 3) * m.at(2, 2);

        inv.at(1, 1) = m.at(0, 0) * m.at(2, 2) * m.at(3, 3) - m.at(0, 0) * m.at(2, 3) * m.at(3, 2) -
            m.at(2, 0) * m.at(0, 2) * m.at(3, 3) + m.at(2, 0) * m.at(0, 3) * m.at(3, 2) +
            m.at(3, 0) * m.at(0, 2) * m.at(2, 3) - m.at(3, 0) * m.at(0, 3) * m.at(2, 2);

        inv.at(2, 1) = -m.at(0, 0) * m.at(2, 1) * m.at(3, 3) + m.at(0, 0) * m.at(2, 3) * m.at(3, 1) +
            m.at(2, 0) * m.at(0, 1) * m.at(3, 3) - m.at(2, 0) * m.at(0, 3) * m.at(3, 1) -
            m.at(3, 0) * m.at(0, 1) * m.at(2, 3) + m.at(3, 0) * m.at(0, 3) * m.at(2, 1);

        inv.at(3, 1) = m.at(0, 0) * m.at(2, 1) * m.at(3, 2) - m.at(0, 0) * m.at(2, 2) * m.at(3, 1) -
            m.at(2, 0) * m.at(0, 1) * m.at(3, 2) + m.at(2, 0) * m.at(0, 2) * m.at(3, 1) +
            m.at(3, 0) * m.at(0, 1) * m.at(2, 2) - m.at(3, 0) * m.at(0, 2) * m.at(2, 1);

        inv.at(0, 2) = m.at(0, 1) * m.at(1, 2) * m.at(3, 3) - m.at(0, 1) * m.at(1, 3) * m.at(3, 2) -
            m.at(1, 1) * m.at(0, 2) * m.at(3, 3) + m.at(1, 1) * m.at(0, 3) * m.at(3, 2) +
            m.at(3, 1) * m.at(0, 2) * m.at(1, 3) - m.at(3, 1) * m.at(0, 3) * m.at(1, 2);

        inv.at(1, 2) = -m.at(0, 0) * m.at(1, 2) * m.at(3, 3) + m.at(0, 0) * m.at(1, 3) * m.at(3, 2) +
            m.at(1, 0) * m.at(0, 2) * m.at(3, 3) - m.at(1, 0) * m.at(0, 3) * m.at(3, 2) -
            m.at(3, 0) * m.at(0, 2) * m.at(1, 3) + m.at(3, 0) * m.at(0, 3) * m.at(1, 2);

        inv.at(2, 2) = m.at(0, 0) * m.at(1, 1) * m.at(3, 3) - m.at(0, 0) * m.at(1, 3) * m.at(3, 1) -
            m.at(1, 0) * m.at(0, 1) * m.at(3, 3) + m.at(1, 0) * m.at(0, 3) * m.at(3, 1) +
            m.at(3, 0) * m.at(0, 1) * m.at(1, 3) - m.at(3, 0) * m.at(0, 3) * m.at(1, 1);

        inv.at(3, 2) = -m.at(0, 0) * m.at(1, 1) * m.at(3, 2) + m.at(0, 0) * m.at(1, 2) * m.at(3, 1) +
            m.at(1, 0) * m.at(0, 1) * m.at(3, 2) - m.at(1, 0) * m.at(0, 2) * m.at(3, 1) -
            m.at(3, 0) * m.at(0, 1) * m.at(1, 2) + m.at(3, 0) * m.at(0, 2) * m.at(1, 1);

        inv.at(0, 3) = -m.at(0, 1) * m.at(1, 2) * m.at(2, 3) + m.at(0, 1) * m.at(1, 3) * m.at(2, 2) +
            m.at(1, 1) * m.at(0, 2) * m.at(2, 3) - m.at(1, 1) * m.at(0, 3) * m.at(2, 2) -
            m.at(2, 1) * m.at(0, 2) * m.at(1, 3) + m.at(2, 1) * m.at(0, 3) * m.at(1, 2);

        inv.at(1, 3) = m.at(0, 0) * m.at(1, 2) * m.at(2, 3) - m.at(0, 0) * m.at(1, 3) * m.at(2, 2) -
            m.at(1, 0) * m.at(0, 2) * m.at(2, 3) + m.at(1, 0) * m.at(0, 3) * m.at(2, 2) +
            m.at(2, 0) * m.at(0, 2) * m.at(1, 3) - m.at(2, 0) * m.at(0, 3) * m.at(1, 2);

        inv.at(2, 3) = -m.at(0, 0) * m.at(1, 1) * m.at(2, 3) + m.at(0, 0) * m.at(1, 3) * m.at(2, 1) +
            m.at(1, 0) * m.at(0, 1) * m.at(2, 3) - m.at(1, 0) * m.at(0, 3) * m.at(2, 1) -
            m.at(2, 0) * m.at(0, 1) * m.at(1, 3) + m.at(2, 0) * m.at(0, 3) * m.at(1, 1);

        inv.at(3, 3) = m.at(0, 0) * m.at(1, 1) * m.at(2, 2) - m.at(0, 0) * m.at(1, 2) * m.at(2, 1) -
            m.at(1, 0) * m.at(0, 1) * m.at(2, 2) + m.at(1, 0) * m.at(0, 2) * m.at(2, 1) +
            m.at(2, 0) * m.at(0, 1) * m.at(1, 2) - m.at(2, 0) * m.at(0, 2) * m.at(1, 1);

        const f32 det = m.at(0, 0) * inv.at(0, 0) + m.at(0, 1) * inv.at(1, 0) + m.at(0, 2) * inv.at(2, 0) +
            m.at(0, 3) * inv.at(3, 0);

        if (det <= epsilon && det >= -epsilon)
        {
            return "Math operation failed"_err;
        }

        const f32 invDet = 1.f / det;

        if (outDeterminant)
        {
            *outDeterminant = det;
        }

        return inv * invDet;
    }
}