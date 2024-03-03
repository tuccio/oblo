#pragma once

#include <oblo/math/aabb.hpp>
#include <oblo/math/frustum.hpp>

namespace oblo
{
    // Returns true iff the frustum intersects or contains the aabb
    constexpr bool intersects_or_contains(const frustum& f, const aabb& box)
    {
        const vec3* minMax[] = {&box.min, &box.max};

        for (const plane& plane : f.planes)
        {
            const u32 nx = u32{plane.normal.x < 0.f};
            const u32 ny = u32{plane.normal.y < 0.f};
            const u32 nz = u32{plane.normal.z < 0.f};

            const vec3 p = vec3{minMax[nx]->x, minMax[ny]->y, minMax[nz]->z};
            const f32 d = dot(plane.normal, p);

            if (d > -plane.offset)
            {
                return false;
            }
        }

        return true;
    }
}