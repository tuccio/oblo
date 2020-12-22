#pragma once

#include <oblo/math/constants.hpp>
#include <oblo/math/vec3.hpp>

#include <random>

namespace oblo
{
    template <typename RandomEngine>
    vec3 hemisphere_uniform_sample(RandomEngine& rng, const vec3& normal)
    {
        std::uniform_real_distribution<float> dist{-1.f, 1.f};
        vec3 x{dist(rng), dist(rng), dist(rng)};

        while (dot(x, x) > 1.f)
        {
            x = vec3{dist(rng), dist(rng), dist(rng)};
        }

        if (dot(normal, x) < 0.f)
        {
            x = -x;
        }

        return normalize(x);
    }
}