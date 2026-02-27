#ifndef SURFEL_GRID_DATA_R
#define SURFEL_GRID_DATA_R

#define SURFEL_GRID_DATA_QUALIFIER readonly

#include <surfels/buffers/surfel_grid_data_buffer>

struct surfel_grid_cell_iterator
{
    uint index;
    uint begin;
    uint end;
};

surfel_grid_cell_iterator surfel_grid_cell_iterator_begin(in surfel_grid_cell cell)
{
    surfel_grid_cell_iterator it;
    it.index = cell.surfelsBegin;
    it.begin = cell.surfelsBegin;
    it.end = cell.surfelsBegin + cell.surfelsCount;
    return it;
}

bool surfel_grid_cell_iterator_has_next(in surfel_grid_cell_iterator it)
{
    return it.index != it.end;
}

uint surfel_grid_cell_iterator_count(in surfel_grid_cell_iterator it)
{
    return it.end - it.begin;
}

void surfel_grid_cell_iterator_advance(inout surfel_grid_cell_iterator it)
{
    ++it.index;
}

uint surfel_grid_cell_iterator_get(in surfel_grid_cell_iterator it)
{
    return g_SurfelGridCellData[it.index];
}

#endif