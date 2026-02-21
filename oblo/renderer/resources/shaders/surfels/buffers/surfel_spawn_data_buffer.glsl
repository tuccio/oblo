#ifndef OBLO_INCLUDE_SURFELS_SURFEL_SPAWN_DATA_BUFFER
#define OBLO_INCLUDE_SURFELS_SURFEL_SPAWN_DATA_BUFFER

#include <surfels/buffers/surfel_buffer_bindings>
#include <surfels/surfel_data>

#if !defined(SURFEL_SPAWN_DATA_BINDING) || !defined(SURFEL_SPAWN_DATA_QUALIFIER)
    #error "Binding and memory qualifier must be defined before including this header"
#endif

layout(std430, binding = SURFEL_SPAWN_DATA_BINDING) restrict SURFEL_SPAWN_DATA_QUALIFIER buffer b_SurfelsSpawnData
{
    surfel_spawn_data g_SurfelSpawnData[];
};

#endif