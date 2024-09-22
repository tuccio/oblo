#ifndef OBLO_INCLUDE_SURFELS_SURFEL_DATA
#define OBLO_INCLUDE_SURFELS_SURFEL_DATA

const uint SURFEL_INVALID = 0xFFFFFFFF;

struct surfel_data
{
    uint nextInCell;
};

struct surfel_grid_header
{
    vec3 boundsMin;
    uint cellsCountX;
    vec3 boundsMax;
    uint cellsCountY;
    vec3 cellSize;
    uint cellsCountZ;
};

struct surfel_grid_cell
{
    uint firstSurfel;
};

struct surfel_stack_header
{
    uint available;
};

struct surfel_stack_entry
{
    uint surfelId;
};

uvec3 surfel_grid_cells_count(in surfel_grid_header h)
{
    return uvec3(h.cellsCountX, h.cellsCountY, h.cellsCountZ);
}

uint surfel_grid_cell_index(in surfel_grid_header h, in uvec3 cell)
{
    return cell.x + cell.y * h.cellsCountX + cell.z * h.cellsCountX * h.cellsCountY;
}

#endif