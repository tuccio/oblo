#ifndef OBLO_INCLUDE_SURFELS_SURFEL_DATA
#define OBLO_INCLUDE_SURFELS_SURFEL_DATA

#include <ecs/entity>
#include <renderer/constants>

// Used as a coverage value for surfel_tile_data when no geometry is present
const float NO_SURFELS_NEEDED = 1e6;

// Number of rays we can shoot for a single surfel each frame
const uint SURFEL_MAX_RAYS_PER_SURFEL = 128;

// When gathering irradiance from surfels, we share irradiance from neighbors up to a certain distance
// This is the scale we use to multiply the radius to determine the distance
const float SURFEL_SHARING_SCALE = 4;
const float SURFEL_SHARING_SCALE2 = SURFEL_SHARING_SCALE * SURFEL_SHARING_SCALE;

// When spawning surfels, or calculating overcoverage for killing them as we move away, we weight surfel sharing
// contribution, but use some extra tolerance to make sure coverage is good and that we don't kill surfels too soon
const float SURFEL_SHARING_SPAWN_SCALE = SURFEL_SHARING_SCALE * .85f;

// Surfel radius at 1 meter distance from the camera
const float SURFEL_RADIUS_SCALE = 0.04;

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
    uint currentTimestamp;
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
    uint numSamples;
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

float surfel_grid_max_contribution_distance(in surfel_grid_header h)
{
    return 32;
    // return h.cellSize;
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

float surfel_clamp_radius(in surfel_grid_header gridHeader, in float radius)
{
    // The max radius determines in how many cells the surfel might be replicated, updating the maximum radius might
    // require updating g_MaxSurfelMultiplicity in C++
    const float gridCellSize = surfel_grid_cell_size(gridHeader);
    const float maxRadius = .25f * gridCellSize;
    const float minRadius = .05f * gridCellSize;

    return max(minRadius, min(maxRadius, radius));
}

float surfel_estimate_radius(in surfel_grid_header gridHeader, in vec3 cameraPosition, in vec3 surfelPosition)
{
    const vec3 cameraVector = surfelPosition - cameraPosition;
    const float cameraDistance2 = dot(cameraVector, cameraVector);

    const float gridCellSize = surfel_grid_cell_size(gridHeader);

    const float radius = surfel_clamp_radius(gridHeader, SURFEL_RADIUS_SCALE * sqrt(cameraDistance2));

    return radius;
}

surfel_lighting_data surfel_lighting_data_new()
{
    surfel_lighting_data r;
    r.irradiance = vec3(0);
    r.numSamples = 0;
    return r;
}

surfel_light_estimator_data surfel_light_estimator_data_new()
{
    surfel_light_estimator_data r;
    r.shortTermMean = vec3(0);
    r.varianceBasedBlendReduction = 0;
    r.variance = vec3(0);
    r.inconsistency = 0;
    return r;
}

#endif