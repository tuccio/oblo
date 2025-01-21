#ifndef OBLO_INCLUDE_SURFELS_CONTRIBUTION
#define OBLO_INCLUDE_SURFELS_CONTRIBUTION

#include <surfels/buffers/surfel_data_r>
#include <surfels/buffers/surfel_grid_data_r>
#include <surfels/buffers/surfel_grid_r>
#include <surfels/buffers/surfel_lighting_data_in_r>

vec3 surfel_calculate_contribution(in vec3 position, in vec3 normal)
{
    vec3 irradiance = vec3(0);

    const ivec3 cell = surfel_grid_find_cell(g_SurfelGridHeader, position);

    if (surfel_grid_has_cell(g_SurfelGridHeader, cell))
    {
        vec3 allSum = vec3(0);
        float weightSum = 0;

        const uint cellIndex = surfel_grid_cell_index(g_SurfelGridHeader, cell);

        const surfel_grid_cell gridCell = g_SurfelGridCells[cellIndex];

        surfel_grid_cell_iterator cellIt = surfel_grid_cell_iterator_begin(gridCell);

        for (; surfel_grid_cell_iterator_has_next(cellIt); surfel_grid_cell_iterator_advance(cellIt))
        {
            const uint surfelId = surfel_grid_cell_iterator_get(cellIt);
            const surfel_data surfel = g_SurfelData[surfelId];

            const surfel_lighting_data surfelLight = g_InSurfelsLighting[surfelId];

            const vec3 surfelPosition = surfel_data_world_position(surfel);
            const vec3 surfelNormal = surfel_data_world_normal(surfel);

            const vec3 pToS = surfelPosition - position;

            const float distance2 = dot(pToS, pToS);

            const float radius2 = surfel.radius * surfel.radius;

            // We allow influences up to this distance
            const float threshold = SURFEL_CONTRIBUTION_THRESHOLD_SQR * radius2;

            const float angleWeight = max(dot(surfelNormal, normal), 0);
            const vec3 surfelContribution = angleWeight * surfelLight.irradiance;
            allSum += surfelContribution;

            if (distance2 <= threshold)
            {
                const float weight = angleWeight * (1 - distance2 / threshold);
                weightSum += weight;

                irradiance += weight * surfelContribution;
            }
        }

        const vec3 weightedAvg = weightSum < 1e-1 ? vec3(0) : irradiance / weightSum;

        if (weightSum < 1)
        {
            const vec3 gridAvg = allSum / max(1, surfel_grid_cell_iterator_count(cellIt));
            irradiance = mix(gridAvg, weightedAvg, weightSum);
        }
        else
        {
            irradiance = weightedAvg;
        }
    }

    return irradiance;
}

#endif