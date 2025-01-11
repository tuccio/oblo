#ifndef OBLO_INCLUDE_SURFELS_SURFEL_LIGHTING_DATA_OUT_W
#define OBLO_INCLUDE_SURFELS_SURFEL_LIGHTING_DATA_OUT_W

#include <surfels/buffers/surfel_buffer_bindings>
#include <surfels/surfel_data>

layout(std430, binding = SURFEL_LIGHTING_DATA_OUT_BINDING) restrict writeonly buffer b_OutSurfelsLighting
{
    surfel_lighting_data g_OutSurfelsLighting[];
};

#endif