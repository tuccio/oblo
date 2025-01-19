#ifndef OBLO_INCLUDE_SURFELS_GRID_UTILS
#define OBLO_INCLUDE_SURFELS_GRID_UTILS

#include <surfels/surfel_data>

void surfel_commit_to_grid(in uint surfelId, in uint cellIndex);

void surfel_scatter_to_grid(
    in surfel_grid_header gridHeader, in ivec3 cell, in uint surfelId, in vec3 positionWS, in float radius)
{
    if (surfel_grid_has_cell(gridHeader, cell))
    {
        const uint cellIndex = surfel_grid_cell_index(gridHeader, cell);
        surfel_commit_to_grid(surfelId, cellIndex);
    }

    // Search neighboring cells and accumulate overlapping surfels
    [[unroll]] for (int x = -1; x <= 1; ++x)
    {
        [[unroll]] for (int y = -1; y <= 1; ++y)
        {
            [[unroll]] for (int z = -1; z <= 1; ++z)
            {
                if (x == 0 && y == 0 && z == 0)
                {
                    // Skip the cell we already dealt with
                    continue;
                }

                const vec3 delta = vec3(radius) * vec3(x, y, z);
                const vec3 posOnSurface = positionWS + delta;

                const ivec3 neighboringCell = surfel_grid_find_cell(gridHeader, posOnSurface);

                if (neighboringCell != cell)
                {
                    const uint neighboringCellIndex = surfel_grid_cell_index(gridHeader, neighboringCell);
                    surfel_commit_to_grid(surfelId, neighboringCellIndex);
                }
            }
        }
    }
}

#endif