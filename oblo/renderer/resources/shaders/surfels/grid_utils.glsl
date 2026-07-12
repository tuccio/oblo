#ifndef OBLO_INCLUDE_SURFELS_GRID_UTILS
#define OBLO_INCLUDE_SURFELS_GRID_UTILS

#include <surfels/surfel_data>

void surfel_commit_to_grid(in uint surfelId, in ivec3 cell);

uint surfel_scatter_to_grid(
    in surfel_grid_header gridHeader, in ivec3 cell, in uint surfelId, in vec3 positionWS, in float radius)
{
    uint numInserts = 0;

    const vec3 coordsMin = positionWS - radius * SURFEL_SHARING_SCALE;
    const vec3 coordsMax = positionWS + radius * SURFEL_SHARING_SCALE;

    const ivec3 cellMin = surfel_grid_find_cell(gridHeader, coordsMin);
    const ivec3 cellMax = surfel_grid_find_cell(gridHeader, coordsMax);

    for (int x = cellMin.x; x <= cellMax.x; ++x)
    {
        for (int y = cellMin.y; y <= cellMax.y; ++y)
        {
            for (int z = cellMin.z; z <= cellMax.z; ++z)
            {
                const ivec3 neighboringCell = ivec3(x, y, z);

                if (!surfel_grid_has_cell(gridHeader, neighboringCell))
                {
                    continue;
                }

                surfel_commit_to_grid(surfelId, neighboringCell);
                ++numInserts;
            }
        }
    }

    return numInserts;
}

#endif