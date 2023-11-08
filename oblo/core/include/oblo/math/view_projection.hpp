#pragma once

#include <cmath>

#include <oblo/math/angle.hpp>
#include <oblo/math/mat4.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    constexpr mat4 make_perspective_matrix(
        radians verticalFov, float aspectRatio, float n, float f, mat4* inverse = nullptr)
    {
        const float focalLength = 1.f / std::tan(float{verticalFov} * .5f);

        const float x = focalLength * aspectRatio;
        const float y = -focalLength;
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

    constexpr mat4 make_look_at(const vec3& position, const vec3& up, const vec3& target)
    {
        const vec3 z = normalize(position - target);
        const vec3 x = normalize(cross(up, z));
        const vec3 y = cross(z, x);

        return mat4({
            {x.x, x.y, x.z, -dot(x, position)},
            {y.x, y.y, y.z, -dot(y, position)},
            {z.x, z.y, z.z, -dot(z, position)},
            {0.f, 0.f, 0.f, 1.f},
        });
    }
}