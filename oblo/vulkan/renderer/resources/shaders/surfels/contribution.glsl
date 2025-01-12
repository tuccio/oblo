#ifndef OBLO_INCLUDE_SURFELS_CONTRIBUTION
#define OBLO_INCLUDE_SURFELS_CONTRIBUTION

#include <surfels/buffers/surfel_data_r>
#include <surfels/buffers/surfel_grid_r>
#include <surfels/buffers/surfel_lighting_data_in_r>

vec3 surfel_calculate_contribution(in vec3 position, in vec3 normal)
{
    vec3 radiance = vec3(0);
    const ivec3 cell = surfel_grid_find_cell(g_SurfelGridHeader, position);

    if (surfel_grid_has_cell(g_SurfelGridHeader, cell))
    {
        const uint cellIndex = surfel_grid_cell_index(g_SurfelGridHeader, cell);

        const surfel_grid_cell gridCell = g_SurfelGridCells[cellIndex];
        const uint surfelsCount = min(SURFEL_MAX_PER_CELL, gridCell.surfelsCount);

        vec3 radianceSum = vec3(0);
        float weightSum = 0.f;

        const sh3 lobe = sh3_cosine_lobe_project(normal);

        for (uint i = 0; i < surfelsCount; ++i)
        {
            const uint surfelId = gridCell.surfels[i];

            const surfel_data surfel = g_SurfelData[surfelId];

            const vec3 positionToSurfel = surfel_data_world_position(surfel) - position;
            const float distance2 = dot(positionToSurfel, positionToSurfel);
            const float radius2 = surfel.radius * surfel.radius;

            const surfel_lighting_data surfelLight = g_InSurfelsLighting[surfelId];

            // Integral of the product of cosine and the irradiance
            const float r = sh_dot(lobe, surfelLight.shRed);
            const float g = sh_dot(lobe, surfelLight.shGreen);
            const float b = sh_dot(lobe, surfelLight.shBlue);

            // if (distance2 <= radius2)
            // {
            //     // return vec3(1);
            //     return vec3(r, g, b);
            // }

            const float weight = max(0, 4 * radius2 - distance2);
            // const float weight = 1.f / surfelsCount;
            // const float weight = 1;

            radiance.r += weight * r;
            radiance.g += weight * g;
            radiance.b += weight * b;

            radianceSum.r += r;
            radianceSum.g += g;
            radianceSum.b += b;

            weightSum += weight;
        }

        if (weightSum < .5 && surfelsCount > 0)
        {
            radiance = radianceSum / surfelsCount;
        }
        else if (weightSum > 1.f)
        {
            const float k = 1 / weightSum;
            radiance = radianceSum / k;
        }

        // We divide by pi to go from irradiance to radiance
        // Could simplify calculations by eliminating pi in the projection coefficients instead
        // c.f. https://seblagarde.wordpress.com/2011/10/09/dive-in-sh-buffer-idea/#more-379
        radiance /= float_pi();
    }

    return radiance;
}

#endif