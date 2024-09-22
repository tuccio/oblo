#ifndef OBLO_INCLUDE_SURFELS_SURFEL_GRID
#define OBLO_INCLUDE_SURFELS_SURFEL_GRID

#include <surfels/surfel_data>

#ifndef SURFEL_GRID_BINDING
    #error "Needs to define the binding before including this header"
#endif

layout(std430, binding = SURFEL_GRID_BINDING) restrict writeonly buffer b_SurfelsGrid
{
    surfel_grid_header g_SurfelGridHeader;
    surfel_grid_cell g_SurfelGridCells[];
};

#endif