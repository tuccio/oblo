#ifndef OBLO_INCLUDE_SURFELS_SURFEL_GRID_DATA_BUFFER
#define OBLO_INCLUDE_SURFELS_SURFEL_GRID_DATA_BUFFER

#include <surfels/buffers/surfel_buffer_bindings>
#include <surfels/surfel_data>

#if !defined(SURFEL_GRID_DATA_BINDING) || !defined(SURFEL_GRID_DATA_QUALIFIER)
    #error "Binding and memory qualifier must be defined before including this header"
#endif

layout(std430, binding = SURFEL_GRID_DATA_BINDING) restrict SURFEL_GRID_DATA_QUALIFIER buffer b_SurfelsGridData
{
    uint g_SurfelGridCellDataIndex;
    uint g_SurfelGridCellData[];
};

#endif