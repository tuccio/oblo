#ifndef OBLO_INCLUDE_RENDERER_INTERSECTION
#define OBLO_INCLUDE_RENDERER_INTERSECTION

#include <renderer/geometry/volumes>

bool intersects_or_contains(in frustum f, in aabb box)
{
    const vec3 minMax[2] = {box.min, box.max};

    for (uint i = 0; i < 6; ++i)
    {
        const plane plane = f.planes[i];

        const uint nx = plane.normal.x < 0.f ? 1 : 0;
        const uint ny = plane.normal.y < 0.f ? 1 : 0;
        const uint nz = plane.normal.z < 0.f ? 1 : 0;

        const vec3 p = vec3(minMax[nx].x, minMax[ny].y, minMax[nz].z);
        const float d = dot(plane.normal, p);

        if (d > -plane.offset)
        {
            return false;
        }
    }

    return true;
}

#endif
