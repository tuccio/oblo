#ifndef OBLO_INCLUDE_SURFELS_SURFEL_STACK
#define OBLO_INCLUDE_SURFELS_SURFEL_STACK

#include <surfels/buffers/surfel_buffer_bindings>
#include <surfels/surfel_data>

#if !defined(SURFEL_STACK_BINDING) || !defined(SURFEL_STACK_QUALIFIER)
    #error "Binding and memory qualified must be defined before including this header"
#endif

layout(std430, binding = SURFEL_STACK_BINDING) restrict SURFEL_STACK_QUALIFIER buffer b_SurfelsStack
{
    surfel_stack_header g_SurfelStackHeader;
    surfel_stack_entry g_SurfelStackEntries[];
};

#endif