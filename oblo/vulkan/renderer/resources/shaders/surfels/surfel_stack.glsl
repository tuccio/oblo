#ifndef OBLO_INCLUDE_SURFELS_SURFEL_GRID
#define OBLO_INCLUDE_SURFELS_SURFEL_GRID

#include <surfels/surfel_data>

#if !defined(SURFEL_STACK_BINDING) || !defined(SURFEL_STACK_QUALIFIER)
    #error "Binding and memory qualified must be defined before including this header"
#endif

layout(std430, binding = SURFEL_STACK_BINDING) restrict SURFEL_STACK_QUALIFIER buffer b_SurfelsStack
{
    surfel_stack_header g_SurfelStackHeader;
    surfel_stack_entry g_SurfelStackEntries[];
};

uint surfel_stack_allocate()
{
    const int previous = atomicAdd(g_SurfelStackHeader.available, -1);

    // This means no surfels are available and allocation failed
    // Before returning an invalid surfel we undo our subtraction
    if (previous <= 0)
    {
        atomicAdd(g_SurfelStackHeader.available, 1);
        return SURFEL_INVALID;
    }

    return uint(previous - 1);
}

#endif