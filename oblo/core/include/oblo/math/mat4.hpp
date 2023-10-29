#pragma once

#include <cmath>
#include <oblo/core/types.hpp>
#include <oblo/math/vec4.hpp>

namespace oblo
{
    struct mat4
    {
        vec4 rows[4];

        constexpr vec4& operator[](u32 row)
        {
            return rows[row];
        }

        constexpr const vec4& operator[](u32 row) const
        {
            return rows[row];
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
                    d += lhs[i][k] * rhs[k][i];
                }

                r[i][j] = d;
            }
        }

        return r;
    }
}