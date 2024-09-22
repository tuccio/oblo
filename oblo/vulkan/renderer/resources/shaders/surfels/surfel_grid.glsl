#ifndef OBLO_INCLUDE_SURFELS_SURFEL_GRID
#define OBLO_INCLUDE_SURFELS_SURFEL_GRID

#include <surfels/surfel_data>

#if !defined(SURFEL_GRID_BINDING) || !defined(SURFEL_GRID_QUALIFIER)
    #error "Binding and memory qualified must be defined before including this header"
#endif

layout(std430, binding = SURFEL_GRID_BINDING) restrict SURFEL_GRID_QUALIFIER buffer b_SurfelsGrid
{
    surfel_grid_header g_SurfelGridHeader;
    surfel_grid_cell g_SurfelGridCells[];
};

#endif