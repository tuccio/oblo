#ifndef OBLO_INCLUDE_SURFELS_SURFEL_POOL
#define OBLO_INCLUDE_SURFELS_SURFEL_POOL

#include <surfels/surfel_data>

#if !defined(SURFEL_POOL_BINDING) || !defined(SURFEL_POOL_QUALIFIER)
    #error "Binding and memory qualified must be defined before including this header"
#endif

layout(std430, binding = SURFEL_POOL_BINDING) restrict SURFEL_POOL_QUALIFIER buffer b_SurfelsPool
{
    surfel_data g_SurfelData[];
};

#endif