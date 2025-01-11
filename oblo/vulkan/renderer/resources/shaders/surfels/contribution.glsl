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

vec3 surfel_calculate_contribution(in vec3 position, in vec3 normal)
{
    const ivec3 cell = surfel_grid_find_cell(g_SurfelGridHeader, position);
    vec3 radiance = vec3(0);

    if (surfel_grid_has_cell(g_SurfelGridHeader, cell))
    {
        const uint cellIndex = surfel_grid_cell_index(g_SurfelGridHeader, cell);

        const surfel_grid_cell gridCell = g_SurfelGridCells[cellIndex];
        const uint surfelsCount = min(SURFEL_MAX_PER_CELL, gridCell.surfelsCount);

        vec3 radianceSum = vec3(0);
        float weightSum = 0.f;

        for (uint i = 0; i < surfelsCount; ++i)
        {
            const uint surfelId = gridCell.surfels[i];

            const surfel_data surfel = g_SurfelData[surfelId];

            const vec3 positionToSurfel = surfel_data_world_position(surfel) - position;
            const float distance2 = dot(positionToSurfel, positionToSurfel);
            const float radius2 = surfel.radius * surfel.radius;

            // const float weight = 1.f / surfelsCount;
            const float weight = max(0, 4 * radius2 - distance2);
            // const float weight = 1;

            const surfel_lighting_data surfelLight = g_InSurfelsLighting[surfelId];

            const sh3 lobe = sh3_cosine_lobe_project(normal);
            const float r = sh_dot(lobe, surfelLight.shRed);
            const float g = sh_dot(lobe, surfelLight.shGreen);
            const float b = sh_dot(lobe, surfelLight.shBlue);

            radiance.r += weight * r;
            radiance.g += weight * g;
            radiance.b += weight * b;

            radianceSum.r += r;
            radianceSum.g += g;
            radianceSum.b += b;

            weightSum += weight;
        }

        if (weightSum < 1e-1)
        {
            radiance = radianceSum / surfelsCount;
        }
        else
        {
            const float k = 1 / weightSum;
            radiance *= k;
        }

        // Could simplify calculations by eliminating pi in the projection coefficients instead
        radiance /= float_pi();
    }

    return radiance;
}

#endif