#ifndef OBLO_INCLUDE_SURFELS_SURFEL
#define OBLO_INCLUDE_SURFELS_SURFEL

#include <surfels/surfel_data>

bool surfel_affects(in surfel_data surfel, in vec3 positionWS)
{
    const vec3 surfelPosition = surfel_data_world_position(surfel);
    const vec3 diff = surfelPosition - positionWS;

    const float radius2 = surfel.radius * surfel.radius;

    // TODO: We should probably consider the normal as well, the surfel should not affect anything behind it
    // return dot(diff, diff) <= radius2;
    return true;
}

#endif