#ifndef OBLO_INCLUDE_SURFELS_SURFEL_LIGHTING_DATA_IN_R
#define OBLO_INCLUDE_SURFELS_SURFEL_LIGHTING_DATA_IN_R

#include <surfels/buffers/surfel_buffer_bindings>
#include <surfels/surfel_data>

layout(std430, binding = SURFEL_LIGHTING_DATA_IN_BINDING) restrict readonly buffer b_InSurfelsLighting
{
    surfel_lighting_data g_InSurfelsLighting[];
};

#endif