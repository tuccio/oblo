#ifndef OBLO_INCLUDE_SURFELS_SURFEL_DATA
#define OBLO_INCLUDE_SURFELS_SURFEL_DATA

const uint SURFEL_MAX_PER_CELL = 31;

// Used as a coverage value for surfel_tile_data when no geometry is present
const float NO_SURFELS_NEEDED = 100000.f;

struct surfel_data
{
    vec3 position;
    bool alive;
    vec3 normal;
    float radius;
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
    uint surfelsCount;
    uint surfels[SURFEL_MAX_PER_CELL];
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

ivec3 surfel_grid_find_cell(in surfel_grid_header h, in vec3 positionWS)
{
    const vec3 offset = positionWS - h.boundsMin;
    const vec3 fCell = offset / h.cellSize;
    return ivec3(fCell);
}

bool surfel_grid_has_cell(in surfel_grid_header h, in ivec3 cell)
{
    const ivec3 cellsCount = ivec3(h.cellsCountX, h.cellsCountY, h.cellsCountZ);
    return all(greaterThanEqual(cell, ivec3(0))) && all(lessThan(cell, cellsCount));
}

vec3 surfel_data_world_position(in surfel_data surfel)
{
    return surfel.position;
}

vec3 surfel_data_world_normal(in surfel_data surfel)
{
    return surfel.normal;
}

bool surfel_data_is_alive(in surfel_data surfel)
{
    return surfel.alive;
}

#endif