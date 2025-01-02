#ifndef OBLO_INCLUDE_SURFELS_SURFEL_GRID_BUFFER
#define OBLO_INCLUDE_SURFELS_SURFEL_GRID_BUFFER

#include <surfels/buffers/surfel_buffer_bindings>
#include <surfels/surfel_data>

#if !defined(SURFEL_GRID_BINDING) || !defined(SURFEL_GRID_QUALIFIER)
    #error "Binding and memory qualifier must be defined before including this header"
#endif

layout(std430, binding = SURFEL_GRID_BINDING) restrict SURFEL_GRID_QUALIFIER buffer b_SurfelsGrid
{
    // TODO: Should probably split in 2 different buffers
    surfel_grid_header g_SurfelGridHeader;
    surfel_grid_cell g_SurfelGridCells[];
};

#endif