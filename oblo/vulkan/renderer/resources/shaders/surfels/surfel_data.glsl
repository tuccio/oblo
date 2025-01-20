#ifndef OBLO_INCLUDE_SURFELS_SURFEL_DATA
#define OBLO_INCLUDE_SURFELS_SURFEL_DATA

#include <ecs/entity>
#include <renderer/constants>

// Used as a coverage value for surfel_tile_data when no geometry is present
const float NO_SURFELS_NEEDED = 1e6;
const float SURFEL_CONTRIBUTION_THRESHOLD = 2;
const float SURFEL_CONTRIBUTION_THRESHOLD_SQR = SURFEL_CONTRIBUTION_THRESHOLD * SURFEL_CONTRIBUTION_THRESHOLD;

const uint SURFEL_MAX_RAYS_PER_SURFEL = 64;

struct surfel_spawn_data
{
    ecs_entity entity;
    uint packedMeshletAndTriangleId;
    float barycentricU;
    float barycentricV;
    uint spawnTimestamp;
};

struct surfel_data
{
    vec3 positionWS;
    float radius;
    vec3 normalWS;
    uint requestedRays;
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
    vec3 irradiance;
    float _padding;
};

struct surfel_light_estimator_data
{
    vec3 shortTermMean;
    float varianceBasedBlendReduction;
    vec3 variance;
    float inconsistency;
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

void surfel_data_set_world_normal(inout surfel_data surfel, in vec3 normalWS)
{
    surfel.normalWS = normalWS;
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
    surfelData.requestedRays = 0u;
    return surfelData;
}

bool surfel_data_is_alive(in surfel_data surfelData)
{
    return !isinf(surfelData.positionWS.x);
}

float surfel_max_radius(in surfel_grid_header gridHeader)
{
    const float gridCellSize = surfel_grid_cell_size(gridHeader);
    const float maxRadius = .25f * gridCellSize;
    return maxRadius;
}

float surfel_estimate_radius(in surfel_grid_header gridHeader, in vec3 cameraPosition, in vec3 surfelPosition)
{
    const vec3 cameraVector = surfelPosition - cameraPosition;
    const float cameraDistance2 = dot(cameraVector, cameraVector);

    const float gridCellSize = surfel_grid_cell_size(gridHeader);
    const float surfelScalingFactor = 0.03;

    const float maxRadius = surfel_max_radius(gridHeader);

    const float radius = min(maxRadius, surfelScalingFactor * sqrt(cameraDistance2));

    return radius;
}

surfel_lighting_data surfel_lighting_data_new()
{
    surfel_lighting_data r;
    r.irradiance = vec3(0);
    return r;
}

surfel_light_estimator_data surfel_light_estimator_data_new()
{
    surfel_light_estimator_data r;
    r.shortTermMean = vec3(0);
    r.varianceBasedBlendReduction = 0;
    r.variance = vec3(0);
    r.inconsistency = 1;
    return r;
}

#endif