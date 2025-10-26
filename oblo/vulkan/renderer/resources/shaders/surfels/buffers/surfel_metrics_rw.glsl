#ifndef OBLO_INCLUDE_SURFELS_SURFEL_METRICS_RW
#define OBLO_INCLUDE_SURFELS_SURFEL_METRICS_RW

#include <surfels/buffers/surfel_buffer_bindings>
#include <surfels/surfel_data>

#ifdef SURFEL_METRICS_ENABLED

layout(std430, binding = SURFEL_METRICS_BINDING) restrict buffer b_SurfelsMetrics
{
    surfel_metrics g_SurfelMetrics;
};

#endif

void surfel_metrics_on_primary_ray_cast(in uint numRays)
{
#ifdef SURFEL_METRICS_ENABLED
    atomicAdd(g_SurfelMetrics.primaryRayCasts, numRays);
#endif
}

void surfel_metrics_on_shadow_ray_cast(in uint numRays)
{
#ifdef SURFEL_METRICS_ENABLED
    atomicAdd(g_SurfelMetrics.shadowRayCasts, numRays);
#endif
}

void surfel_metrics_on_spawn(in uint numSpawned)
{
#ifdef SURFEL_METRICS_ENABLED
    atomicAdd(g_SurfelMetrics.surfelsSpawned, numSpawned);
#endif
}

void surfel_metrics_on_alive(in uint numKilled)
{
#ifdef SURFEL_METRICS_ENABLED
    atomicAdd(g_SurfelMetrics.surfelsAlive, numKilled);
#endif
}

void surfel_metrics_on_kill(in uint numKilled)
{
#ifdef SURFEL_METRICS_ENABLED
    atomicAdd(g_SurfelMetrics.surfelsKilled, numKilled);
#endif
}

#endif