#ifndef OBLO_INCLUDE_SURFELS_CONTRIBUTION
#define OBLO_INCLUDE_SURFELS_CONTRIBUTION

#include <surfels/buffers/surfel_data_r>
#include <surfels/buffers/surfel_grid_r>
#include <surfels/buffers/surfel_lighting_data_in_r>

vec3[SURFEL_MAX_PER_CELL] surfel_contributions_at(in vec3 position, out uint count)
{
    vec3 contribution = vec3(0);

    const ivec3 cell = surfel_grid_find_cell(g_SurfelGridHeader, position);

    vec3 contributions[SURFEL_MAX_PER_CELL];

    if (surfel_grid_has_cell(g_SurfelGridHeader, cell))
    {
        const uint cellIndex = surfel_grid_cell_index(g_SurfelGridHeader, cell);

        const surfel_grid_cell gridCell = g_SurfelGridCells[cellIndex];
        const uint surfelsCount = min(SURFEL_MAX_PER_CELL, gridCell.surfelsCount);

        int outIndex = 0;

        for (uint i = 0; i < surfelsCount; ++i)
        {
            const uint surfelId = gridCell.surfels[i];

            // TODO: Radius check, maybe scale the contribution to weight
            const surfel_data surfel = g_SurfelData[surfelId];

            const vec3 positionToSurfel = surfel_data_world_position(surfel) - position;
            const float distance2 = dot(positionToSurfel, positionToSurfel);
            const float radius2 = surfel.radius * surfel.radius;

            if (distance2 > radius2)
            {
                continue;
            }

            contributions[outIndex] = vec3(0.1);
            // contributions[outIndex] = g_InSurfelsLighting[surfelId].radiance;
            ++outIndex;
        }

        count = surfelsCount;
    }
    else
    {
        count = 0;
    }

    return contributions;
}

struct surfel_contribution
{
    vec3 radiance;
};

bool surfel_find_in_range(in vec3 position, out surfel_contribution contribution)
{
    const ivec3 cell = surfel_grid_find_cell(g_SurfelGridHeader, position);

    if (surfel_grid_has_cell(g_SurfelGridHeader, cell))
    {
        const uint cellIndex = surfel_grid_cell_index(g_SurfelGridHeader, cell);

        const surfel_grid_cell gridCell = g_SurfelGridCells[cellIndex];
        const uint surfelsCount = min(SURFEL_MAX_PER_CELL, gridCell.surfelsCount);

        for (uint i = 0; i < surfelsCount; ++i)
        {
            const uint surfelId = gridCell.surfels[i];

            // TODO: Radius check, maybe scale the contribution to weight
            const surfel_data surfel = g_SurfelData[surfelId];

            const vec3 positionToSurfel = surfel_data_world_position(surfel) - position;
            const float distance2 = dot(positionToSurfel, positionToSurfel);
            const float radius2 = surfel.radius * surfel.radius;

            if (distance2 > radius2)
            {
                continue;
            }

            contribution.radiance = g_InSurfelsLighting[surfelId].radiance;
            return true;
        }
    }
    
    return false;
}

#endif