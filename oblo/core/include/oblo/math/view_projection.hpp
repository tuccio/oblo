#pragma once

#include <cmath>

#include <oblo/math/angle.hpp>
#include <oblo/math/mat4.hpp>

namespace oblo
{
    constexpr mat4 make_perspective_matrix(
        radians verticalFov, float aspectRatio, float n, float f, mat4* inverse = nullptr)
    {
        const float focalLength = 1.f / std::tan(float{verticalFov} * .5f);

        const float x = focalLength * aspectRatio;
        const float y = focalLength;
        const float A = n / (f - n);
        const float B = f * A;

        mat4 projection{{
            {x, 0.f, 0.f, 0.f},
            {0.f, y, 0.f, 0.f},
            {0.f, 0.f, A, B},
            {0.f, 0.f, -1.f, 0.f},
        }};

        if (inverse)
        {
            *inverse = mat4{{
                {1.f / x, 0.f, 0.f, 0.f},
                {0.f, 1 / y, 0.f, 0.f},
                {0.f, 0.f, 0.f, -1.f},
                {0.f, 0.f, 1 / B, A / B},
            }};
        }

        return projection;
    }
}