#ifndef OBLO_INCLUDE_SURFELS_SURFEL
#define OBLO_INCLUDE_SURFELS_SURFEL

#include <surfels/surfel_data>

float surfel_weight(in surfel_data surfel, in vec3 positionWS)
{
    const vec3 surfelPosition = surfel_data_world_position(surfel);
    const vec3 diff = surfelPosition - positionWS;

    const float radius2 = surfel.radius * surfel.radius;

    // TODO: We should probably consider the normal as well, the surfel should not affect anything behind it
    return max(radius2 - dot(diff, diff), 0);
    // return max(sqrt(radius2) - length(diff), 0);
}

#endif