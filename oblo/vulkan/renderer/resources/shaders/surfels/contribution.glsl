#ifndef OBLO_INCLUDE_SURFELS_CONTRIBUTION
#define OBLO_INCLUDE_SURFELS_CONTRIBUTION

#include <surfels/buffers/surfel_data_r>
#include <surfels/buffers/surfel_grid_data_r>
#include <surfels/buffers/surfel_grid_r>
#include <surfels/buffers/surfel_lighting_data_in_r>

const uint g_SurfelCandidatesMax = 4;

struct surfel_candidates
{
    uint ids[g_SurfelCandidatesMax];
    float sqrDistances[g_SurfelCandidatesMax];
    uint count;
};

vec3 surfel_calculate_contribution(in vec3 position, in vec3 normal)
{
    vec3 irradiance = vec3(0);
    uint surfelsFound = 0;

    const ivec3 cell = surfel_grid_find_cell(g_SurfelGridHeader, position);

    if (surfel_grid_has_cell(g_SurfelGridHeader, cell))
    {
        const uint cellIndex = surfel_grid_cell_index(g_SurfelGridHeader, cell);

        const surfel_grid_cell gridCell = g_SurfelGridCells[cellIndex];

        for (surfel_grid_cell_iterator cellIt = surfel_grid_cell_iterator_begin(gridCell);
             surfel_grid_cell_iterator_has_next(cellIt);
             surfel_grid_cell_iterator_advance(cellIt))
        {
            const uint surfelId = surfel_grid_cell_iterator_get(cellIt);
            const surfel_data surfel = g_SurfelData[surfelId];

            const vec3 surfelPosition = surfel_data_world_position(surfel);

            const vec3 pToS = surfelPosition - position;

            const float distance2 = dot(pToS, pToS);

            // We allow influences up to this distance
            const float threshold2 = 4 * surfel.radius * surfel.radius;

            if (distance2 <= threshold2)
            {
                const surfel_lighting_data surfelLight = g_InSurfelsLighting[surfelId];
                const vec3 surfelNormal = surfel_data_world_normal(surfel);

                // We should probably weigh based on distance
                irradiance += max(dot(surfelNormal, normal), 0) * surfelLight.irradiance;
                ++surfelsFound;
            }
        }
    }

    if (surfelsFound > 0)
    {
        irradiance /= surfelsFound;
    }

    return irradiance;
}

#endif