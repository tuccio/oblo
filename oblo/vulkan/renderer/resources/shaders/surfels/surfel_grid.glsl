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

#define writeonly 1
#define readonly 2

#if SURFEL_GRID_QUALIFIER != writeonly

uint surfel_grid_cell_index(in ivec3 cell)
{
    return surfel_grid_cell_index(g_SurfelGridHeader, cell);
}

ivec3 surfel_grid_find_cell(in vec3 positionWS)
{
    const vec3 offset = positionWS - g_SurfelGridHeader.boundsMin;
    const vec3 fCell = offset / g_SurfelGridHeader.cellSize;
    return ivec3(fCell);
}

bool surfel_grid_has_cell(in ivec3 cell)
{
    const ivec3 cellsCount =
        ivec3(g_SurfelGridHeader.cellsCountX, g_SurfelGridHeader.cellsCountY, g_SurfelGridHeader.cellsCountZ);

    return all(greaterThanEqual(cell, ivec3(0))) && all(lessThan(cell, cellsCount));
}

#endif

#undef readonly
#undef writeonly

#endif