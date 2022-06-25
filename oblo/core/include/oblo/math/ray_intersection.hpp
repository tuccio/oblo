#pragma once

#include <oblo/math/aabb.hpp>
#include <oblo/math/ray.hpp>
#include <oblo/math/triangle.hpp>

namespace oblo
{
    inline namespace moeller_trumbore
    {
        namespace detail
        {
            template <bool BackfaceCulling>
            constexpr bool intersect(const ray& ray, const triangle& triangle, f32& outDistance)
            {
                constexpr f32 Epsilon = .0000001f;

                const auto v0v1 = triangle.v[1] - triangle.v[0];
                const auto v0v2 = triangle.v[2] - triangle.v[0];

                const auto h = cross(ray.direction, v0v2);
                const auto det = dot(v0v1, h);

                if constexpr (!BackfaceCulling)
                {
                    if (det > -Epsilon && det < Epsilon)
                    {
                        return false;
                    }
                }
                else
                {
                    if (det < Epsilon)
                    {
                        return false;
                    }
                }

                const auto invDet = 1.f / det;
                const auto s = ray.origin - triangle.v[0];

                const auto u = invDet * dot(s, h);

                if (u < 0.f || u > 1.f)
                {
                    return false;
                }

                const auto q = cross(s, v0v1);
                const auto v = invDet * dot(ray.direction, q);

                if (v < 0.f || u + v > 1.f)
                {
                    return false;
                }

                const auto t = invDet * dot(v0v2, q);

                if (t > Epsilon)
                {
                    outDistance = t;
                    return true;
                }

                return false;
            }
        }

        constexpr bool intersect(const ray& ray, const triangle& triangle, f32& outDistance)
        {
            return detail::intersect<false>(ray, triangle, outDistance);
        }

        constexpr bool intersect_cull(const ray& ray, const triangle& triangle, f32& outDistance)
        {
            return detail::intersect<true>(ray, triangle, outDistance);
        }
    }

    constexpr bool intersect(const ray& ray, const aabb& aabb, f32 maxDistance, f32& outT0, f32& outT1)
    {
        f32 t0 = 0.f;
        f32 t1 = maxDistance;

        for (int axis = 0; axis < 3; ++axis)
        {
            f32 invRayDirection = 1.f / ray.direction[axis];

            f32 tNear = (aabb.min[axis] - ray.origin[axis]) * invRayDirection;
            f32 tFar = (aabb.max[axis] - ray.origin[axis]) * invRayDirection;

            if (tNear > tFar)
            {
                std::swap(tNear, tFar);
            }

            t0 = max(t0, tNear);
            t1 = min(t1, tFar);

            if (t0 > t1)
            {
                return false;
            }
        }

        outT0 = t0;
        outT1 = t1;
        return true;
    }
}