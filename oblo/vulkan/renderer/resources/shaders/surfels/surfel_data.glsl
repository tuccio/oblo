#ifndef OBLO_INCLUDE_SURFELS_SURFEL_DATA
#define OBLO_INCLUDE_SURFELS_SURFEL_DATA

#include <ecs/entity>
#include <renderer/constants>
#include <renderer/math/spherical_harmonics>

// Used as a coverage value for surfel_tile_data when no geometry is present
const float NO_SURFELS_NEEDED = 1e6;

// The spherical harmonics type used with surfels
#define surfel_sh sh3

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
    uint globalInstanceId;
};

struct surfel_grid_header
{
    vec3 boundsMin;
    float cellSize;
    vec3 boundsMax;
    uint maxSurfels;
    ivec3 cellsCount;
    uint cellsCountLinearized;
};

struct surfel_grid_cell
{
    uint surfelsCount;
    uint surfelsBegin;
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
    float worstPixelCoverage;
    surfel_spawn_data spawnData;
};

struct surfel_lighting_data
{
    surfel_sh shRed;
    surfel_sh shGreen;
    surfel_sh shBlue;
    uint numSamples;
};

ivec3 surfel_grid_cells_count(in surfel_grid_header h)
{
    return h.cellsCount;
}

float surfel_grid_cell_size(in surfel_grid_header h)
{
    return h.cellSize;
}

uint surfel_grid_cell_index(in surfel_grid_header h, in ivec3 cell)
{
    return cell.x + cell.y * h.cellsCount.x + cell.z * h.cellsCount.x * h.cellsCount.y;
}

ivec3 surfel_grid_find_cell(in surfel_grid_header h, in vec3 positionWS)
{
    const vec3 offset = positionWS - h.boundsMin;
    const vec3 fCell = offset / h.cellSize;
    return ivec3(fCell);
}

bool surfel_grid_has_cell(in surfel_grid_header h, in ivec3 cell)
{
    return all(greaterThanEqual(cell, ivec3(0))) && all(lessThan(cell, surfel_grid_cells_count(h)));
}

vec3 surfel_data_world_position(in surfel_data surfel)
{
    return surfel.positionWS;
}

vec3 surfel_data_world_normal(in surfel_data surfel)
{
    return surfel.normalWS;
}

float surfel_data_world_radius(in surfel_data surfel)
{
    return surfel.radius;
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
    surfelData.globalInstanceId = ~0u;
    return surfelData;
}

bool surfel_data_is_alive(in surfel_data surfelData)
{
    return !isinf(surfelData.positionWS.x);
}

float surfel_estimate_radius(in surfel_grid_header gridHeader, in vec3 cameraPosition, in vec3 surfelPosition)
{
    const vec3 cameraVector = surfelPosition - cameraPosition;
    const float cameraDistance2 = dot(cameraVector, cameraVector);

    const float gridCellSize = surfel_grid_cell_size(gridHeader);
    const float surfelScalingFactor = 0.02;

    const float maxRadius = .5f * gridCellSize - .01f;

    const float radius = min(max(0.f, maxRadius), surfelScalingFactor * sqrt(cameraDistance2));

    return radius;
}

surfel_lighting_data surfel_lighting_data_new()
{
    surfel_lighting_data r;
    sh_zero(r.shRed);
    sh_zero(r.shGreen);
    sh_zero(r.shBlue);
    r.numSamples = 0;
    return r;
}

#endif