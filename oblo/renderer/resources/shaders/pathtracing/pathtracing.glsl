#ifndef OBLO_INCLUDE_PATHTRACING_PATHTRACING
#define OBLO_INCLUDE_PATHTRACING_PATHTRACING

struct pathtracing_payload
{
    vec3 radiance;

    uint depth;
    uint seed;
};

#endif
