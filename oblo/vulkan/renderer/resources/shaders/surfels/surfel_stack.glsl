#ifndef OBLO_INCLUDE_SURFELS_SURFEL_GRID
#define OBLO_INCLUDE_SURFELS_SURFEL_GRID

#include <surfels/surfel_data>

#ifndef SURFEL_STACK_BINDING
    #error "Needs to define the binding before including this header"
#endif

layout(std430, binding = SURFEL_STACK_BINDING) restrict writeonly buffer b_SurfelsStack
{
    surfel_stack_header g_SurfelStackHeader;
    surfel_stack_entry g_SurfelStackEntries[];
};

#endif