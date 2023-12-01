#pragma once

#include <oblo/core/utility.hpp>
#include <oblo/math/vec3.hpp>

#include <limits>
#include <span>

namespace oblo
{
    struct aabb
    {
        vec3 min;
        vec3 max;

        static constexpr aabb make_invalid()
        {
            return {
                .min =
                    {
                        std::numeric_limits<f32>::max(),
                        std::numeric_limits<f32>::max(),
                        std::numeric_limits<f32>::max(),
                    },
                .max =
                    {
                        std::numeric_limits<f32>::lowest(),
                        std::numeric_limits<f32>::lowest(),
                        std::numeric_limits<f32>::lowest(),
                    },
            };
        }
    };

    constexpr aabb extend(const aabb& lhs, const aabb& rhs)
    {
        return {min(lhs.min, rhs.min), max(lhs.max, rhs.max)};
    }

    constexpr u8 max_extent(const aabb& aabb)
    {
        const auto diagonal = aabb.max - aabb.min;

        if (diagonal.x > diagonal.y && diagonal.x > diagonal.z)
        {
            return 0;
        }
        else if (diagonal.y > diagonal.z)
        {
            return 1;
        }
        else
        {
            return 2;
        }
    }

    constexpr aabb compute_aabb(std::span<const aabb> aabbs)
    {
        aabb current = aabb::make_invalid();

        for (auto& aabb : aabbs)
        {
            current = extend(current, aabb);
        }

        return current;
    }

    constexpr aabb compute_aabb(std::span<const vec3> points)
    {
        aabb current = aabb::make_invalid();

        for (auto& point : points)
        {
            current = extend(current, aabb{point, point});
        }

        return current;
    }

    constexpr bool is_valid(const aabb& aabb)
    {
        const auto& [min, max] = aabb;
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }
}