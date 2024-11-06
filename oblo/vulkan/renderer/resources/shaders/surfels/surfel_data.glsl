#ifndef OBLO_INCLUDE_SURFELS_SURFEL_DATA
#define OBLO_INCLUDE_SURFELS_SURFEL_DATA

const uint SURFEL_INVALID = 0xFFFFFFFF;

struct surfel_data
{
    vec3 position;
    vec3 normal;
    uint nextInCell;
};

struct surfel_grid_header
{
    vec3 boundsMin;
    int cellsCountX;
    vec3 boundsMax;
    int cellsCountY;
    vec3 cellSize;
    int cellsCountZ;
};

struct surfel_grid_cell
{
    uint firstSurfel;
};

struct surfel_stack_header
{
    int available;
};

struct surfel_stack_entry
{
    uint surfelId;
};

struct surfel_tile_data
{
    vec3 position;
    vec3 normal;
    float coverage;
};

ivec3 surfel_grid_cells_count(in surfel_grid_header h)
{
    return ivec3(h.cellsCountX, h.cellsCountY, h.cellsCountZ);
}

uint surfel_grid_cell_index(in surfel_grid_header h, in ivec3 cell)
{
    return cell.x + cell.y * h.cellsCountX + cell.z * h.cellsCountX * h.cellsCountY;
}

vec3 surfel_data_world_position(in surfel_data surfel)
{
    return surfel.position;
}

vec3 surfel_data_world_normal(in surfel_data surfel)
{
    return surfel.normal;
}

#endif