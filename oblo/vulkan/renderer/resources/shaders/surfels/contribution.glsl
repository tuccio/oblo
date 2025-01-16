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

vec3 surfel_calculate_contribution(in vec3 cameraPosition, in vec3 position, in vec3 normal)
{
    vec3 radiance = vec3(0);

    const ivec3 baseCell = surfel_grid_find_cell(g_SurfelGridHeader, position);

    const float searchRadius = surfel_estimate_radius(g_SurfelGridHeader, cameraPosition, position);
    const float threshold = 4 * searchRadius * searchRadius;

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

                        if (distance2 <= threshold)
                        {
                            const surfel_lighting_data surfelLight = g_InSurfelsLighting[surfelId];

                            surfel_sh lobe;
                            sh_cosine_lobe_project(lobe, normal);

                            const float multiplier = surfelLight.numSamples == 0 ? 0.f : 1.f / surfelLight.numSamples;

                            // Integral of the product of cosine and the irradiance
                            const float r = multiplier * sh_dot(lobe, surfelLight.shRed);
                            const float g = multiplier * sh_dot(lobe, surfelLight.shGreen);
                            const float b = multiplier * sh_dot(lobe, surfelLight.shBlue);

                            radiance += vec3(r, g, b);
                            ++surfelsFound;
                        }
                    }
                }
            }
        }
    }

    if (surfelsFound > 0)
    {
        radiance /= surfelsFound;
    }

    return radiance;
}

vec3 surfel_calculate_contribution_single_cell_2(in vec3 position, in vec3 normal)
{
    const surfel_candidates candidates = surfel_fetch_best_candidates(position);

    if (candidates.count == 0)
    {
        return vec3(0);
    }

    vec3 radiance = vec3(0);
    float weightSum = 0.f;

    surfel_sh lobe;
    sh_cosine_lobe_project(lobe, normal);

    for (uint i = 0; i < candidates.count; ++i)
    {
        const uint surfelId = candidates.ids[i];

        const surfel_lighting_data surfelLight = g_InSurfelsLighting[surfelId];

        const float multiplier = surfelLight.numSamples == 0 ? 0.f : 1.f / surfelLight.numSamples;

        // Integral of the product of cosine and the irradiance
        const float r = multiplier * sh_dot(lobe, surfelLight.shRed);
        const float g = multiplier * sh_dot(lobe, surfelLight.shGreen);
        const float b = multiplier * sh_dot(lobe, surfelLight.shBlue);

#if 0
        const float gridCellSize = surfel_grid_cell_size(gridHeader);
        const float weight = gridCellSize - sqrt(candidates.sqrDistances[i]);
#else
        const float weight = 1.f;
#endif
        radiance += vec3(r, g, b) * weight;

        weightSum += weight;
    }

    radiance /= weightSum;

    return radiance;
}

vec3 surfel_calculate_contribution_single_cell(in vec3 position, in vec3 normal)
{
    vec3 radiance = vec3(0);

    const ivec3 cell = surfel_grid_find_cell(g_SurfelGridHeader, position);

    uint minSurfelId = g_SurfelGridHeader.maxSurfels;

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

            if (distance2 <= radius2)
            {
                minSurfelId = min(minSurfelId, surfelId);
            }
        }

        if (minSurfelId < g_SurfelGridHeader.maxSurfels)
        {
            const uint surfelId = minSurfelId;

            const surfel_lighting_data surfelLight = g_InSurfelsLighting[surfelId];

            surfel_sh lobe;
            sh_cosine_lobe_project(lobe, normal);

            const float multiplier = surfelLight.numSamples == 0 ? 0.f : 1.f / surfelLight.numSamples;

            // Integral of the product of cosine and the irradiance
            const float r = multiplier * sh_dot(lobe, surfelLight.shRed);
            const float g = multiplier * sh_dot(lobe, surfelLight.shGreen);
            const float b = multiplier * sh_dot(lobe, surfelLight.shBlue);

            radiance = vec3(r, g, b);
        }
    }

    return radiance;
}

#endif