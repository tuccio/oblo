#ifndef OBLO_INCLUDE_SURFELS_SURFEL_GRID
    #define OBLO_INCLUDE_SURFELS_SURFEL_GRID

    #include <surfels/surfel_data>

    #if !defined(SURFEL_GRID_QUALIFIER)
        #error "Memory qualifier must be defined before including this header"
    #endif

layout(std430, binding = 32) restrict SURFEL_GRID_QUALIFIER buffer b_SurfelsGrid
{
    // TODO: Should probably split in 2 different buffers
    surfel_grid_header g_SurfelGridHeader;
    surfel_grid_cell g_SurfelGridCells[];
};

#else
    #error "Can only include this header once"
#endif