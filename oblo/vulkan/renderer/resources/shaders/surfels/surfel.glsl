#ifndef OBLO_INCLUDE_SURFELS_SURFEL
#define OBLO_INCLUDE_SURFELS_SURFEL

#include <surfels/surfel_data>

const float SURFEL_RADIUS = .5f;
const float SURFEL_RADIUS2 = SURFEL_RADIUS * SURFEL_RADIUS;

bool surfel_affects(in surfel_data surfel, in vec3 positionWS)
{
    const vec3 surfelPosition = surfel_data_world_position(surfel);
    const vec3 diff = surfelPosition - positionWS;

    // We should probably consider the normal as well, the surfel should not affect anything behind it
    return dot(diff, diff) <= SURFEL_RADIUS2;
}

#endif