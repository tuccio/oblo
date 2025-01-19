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

surfel_candidates surfel_fetch_best_candidates(in vec3 position)
{
    surfel_candidates r;
    r.count = 0;

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
            const float radius2 = dot(surfel.radius, surfel.radius);

            if (r.count < g_SurfelCandidatesMax)
            {
                r.ids[r.count] = surfelId;
                r.sqrDistances[r.count] = distance2;
                ++r.count;
            }
            else
            {
                uint worstCandidateIdx = 0;

                for (uint i = 1; i < r.count; ++i)
                {
                    if (r.sqrDistances[i] > r.sqrDistances[worstCandidateIdx])
                    {
                        worstCandidateIdx = i;
                    }
                }

                if (distance2 < r.sqrDistances[worstCandidateIdx])
                {
                    r.ids[worstCandidateIdx] = surfelId;
                    r.sqrDistances[worstCandidateIdx] = surfelId;
                }
            }
        }
    }

    return r;
}

vec3 surfel_calculate_contribution(in vec3 position, in vec3 normal)
{
    vec3 irradiance = vec3(0);

    const ivec3 baseCell = surfel_grid_find_cell(g_SurfelGridHeader, position);

    const float searchRadius = surfel_max_radius(g_SurfelGridHeader);

    uint surfelsFound = 0;

    [[unroll]] for (int x = -1; x <= 1; ++x)
    {
        [[unroll]] for (int y = -1; y <= 1; ++y)
        {
            [[unroll]] for (int z = -1; z <= 1; ++z)
            {
                const ivec3 cell = baseCell + ivec3(x, y, z);

                const vec3 deltaPos = vec3(searchRadius) * vec3(x, y, z);
                const vec3 posOnSurface = position + deltaPos;

                if (surfel_grid_has_cell(g_SurfelGridHeader, cell) &&
                    cell == surfel_grid_find_cell(g_SurfelGridHeader, posOnSurface))
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

                        const float threshold = 4 * surfel.radius * surfel.radius;

                        if (distance2 <= threshold)
                        {
                            const surfel_lighting_data surfelLight = g_InSurfelsLighting[surfelId];
                            const vec3 surfelNormal = surfel_data_world_normal(surfel);

                            irradiance += max(dot(surfelNormal, normal), 0) * surfelLight.irradiance;
                            ++surfelsFound;
                        }
                    }
                }
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