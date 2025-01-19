#ifndef OBLO_INCLUDE_SURFELS_GRID_UTILS
#define OBLO_INCLUDE_SURFELS_GRID_UTILS

#include <surfels/surfel_data>

void surfel_commit_to_grid(in uint surfelId, in uint cellIndex);

void surfel_scatter_to_grid(
    in surfel_grid_header gridHeader, in ivec3 cell, in uint surfelId, in vec3 positionWS, in float radius)
{
    // Search neighboring cells and accumulate overlapping surfels
    [[unroll]] for (int x = -1; x <= 1; ++x)
    {
        [[unroll]] for (int y = -1; y <= 1; ++y)
        {
            [[unroll]] for (int z = -1; z <= 1; ++z)
            {
                const ivec3 neighboringCell = cell + ivec3(x, y, z);

                if (!surfel_grid_has_cell(gridHeader, neighboringCell))
                {
                    continue;
                }

                const vec3 posOnSurface = positionWS + radius * vec3(x, y, z);

                if (neighboringCell == surfel_grid_find_cell(gridHeader, posOnSurface))
                {
                    const uint neighboringCellIndex = surfel_grid_cell_index(gridHeader, neighboringCell);
                    surfel_commit_to_grid(surfelId, neighboringCellIndex);
                }
            }
        }
    }
}

#endif