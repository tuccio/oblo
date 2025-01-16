#ifndef OBLO_INCLUDE_SURFELS_CONTRIBUTION
#define OBLO_INCLUDE_SURFELS_CONTRIBUTION

#include <surfels/buffers/surfel_data_r>
#include <surfels/buffers/surfel_grid_data_r>
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

        surfel_grid_cell_iterator it = surfel_grid_cell_iterator_begin(gridCell);

        vec3 radianceSum = vec3(0);
        float weightSum = 0.f;

        const sh3 lobe = sh3_cosine_lobe_project(normal);

#define INTERPOLATE_SH 1

        sh3 red = sh3_zero();
        sh3 green = sh3_zero();
        sh3 blue = sh3_zero();

        for (; surfel_grid_cell_iterator_has_next(it); surfel_grid_cell_iterator_advance(it))
        {
            const uint surfelId = surfel_grid_cell_iterator_get(it);

            const surfel_data surfel = g_SurfelData[surfelId];

            const vec3 positionToSurfel = surfel_data_world_position(surfel) - position;
            const float distance2 = dot(positionToSurfel, positionToSurfel);
            const float radius2 = surfel.radius * surfel.radius;

            const float contributionThreshold = 4 * surfel.radius;
            const float contributionThreshold2 = contributionThreshold * contributionThreshold;

            // if (distance2 > contributionThreshold2)
            // {
            //     continue;
            // }

            const surfel_lighting_data surfelLight = g_InSurfelsLighting[surfelId];

            // if (distance2 <= radius2)
            // {
            //     // return vec3(1);
            //     return vec3(r, g, b);
            // }

            const float weight = max(0.1f, contributionThreshold2 - distance2);
            // const float weight = max(0.1f, contributionThreshold - sqrt(distance2));
            // const float weight = 1.f / surfelsCount;
            // const float weight = 1;

#if INTERPOLATE_SH
            red = sh_add(red, sh_mul(surfelLight.shRed, weight));
            green = sh_add(sh_mul(surfelLight.shGreen, weight), green);
            blue = sh_add(sh_mul(surfelLight.shBlue, weight), blue);
#else
            // Integral of the product of cosine and the irradiance
            const float r = sh_dot(lobe, surfelLight.shRed);
            const float g = sh_dot(lobe, surfelLight.shGreen);
            const float b = sh_dot(lobe, surfelLight.shBlue);

            radianceSum.r += weight * r;
            radianceSum.g += weight * g;
            radianceSum.b += weight * b;
#endif

            weightSum += weight;
        }

#if INTERPOLATE_SH
        radianceSum.r = sh_dot(lobe, red);
        radianceSum.g = sh_dot(lobe, green);
        radianceSum.b = sh_dot(lobe, blue);
#endif

        if (weightSum > .1f)
        {
            // L1 normalization of weights
            radiance = radianceSum / weightSum;

            // We divide by pi to go from irradiance to radiance
            // Could simplify calculations by eliminating pi in the projection coefficients instead
            // c.f. https://seblagarde.wordpress.com/2011/10/09/dive-in-sh-buffer-idea/#more-379
            // radiance /= float_pi();
            // radiance *= float_pi();
        }
        else
        {
            radiance = vec3(0);
        }
    }

    return radiance;
}

#endif