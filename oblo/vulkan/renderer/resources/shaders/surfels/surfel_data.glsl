#ifndef OBLO_INCLUDE_SURFELS_SURFEL_DATA
#define OBLO_INCLUDE_SURFELS_SURFEL_DATA

#include <ecs/entity>
#include <renderer/constants>

const uint SURFEL_MAX_PER_CELL = 63;
const uint SURFEL_ID_INVALID = -1;

// Used as a coverage value for surfel_tile_data when no geometry is present
const float NO_SURFELS_NEEDED = 10000000.f;

struct surfel_spawn_data
{
    ecs_entity entity;
    uint packedMeshletAndTriangleId;
    float barycentricU;
    float barycentricV;
};

struct surfel_data
{
    vec3 positionWS;
    float radius;
    vec3 normalWS;
    uint nextSurfelId;
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
    uint nextSurfelId;
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
    float averageTileCoverage;
    float worstPixelCoverage;
    bool _hasGeometry;
    float _padding[1];
    surfel_spawn_data spawnData;
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
    return surfel.positionWS;
}

vec3 surfel_data_world_normal(in surfel_data surfel)
{
    return surfel.normalWS;
}

bool surfel_spawn_data_is_alive(in surfel_spawn_data spawnData)
{
    return ecs_entity_is_valid(spawnData.entity);
}

surfel_spawn_data surfel_spawn_data_invalid()
{
    surfel_spawn_data spawnData;
    spawnData.entity = ecs_entity_invalid();
    spawnData.packedMeshletAndTriangleId = -1;
    spawnData.barycentricU = -1.f;
    spawnData.barycentricV = -1.f;
    return spawnData;
}

surfel_data surfel_data_invalid()
{
    surfel_data surfelData;
    surfelData.positionWS = vec3(float_positive_infinity());
    surfelData.normalWS = vec3(float_positive_infinity());
    surfelData.radius = 0.f;
    surfelData.nextSurfelId = SURFEL_ID_INVALID;
    return surfelData;
}

bool surfel_data_is_alive(in surfel_data surfelData)
{
    return !isinf(surfelData.positionWS.x);
}

#endif