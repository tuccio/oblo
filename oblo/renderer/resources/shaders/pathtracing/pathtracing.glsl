#ifndef OBLO_INCLUDE_PATHTRACING_PATHTRACING
#define OBLO_INCLUDE_PATHTRACING_PATHTRACING

struct pathtracing_payload
{
    vec3 radiance;
    vec3 throughput;

    vec3 origin;
    vec3 direction;

    uint seed;

    bool done;
};

#endif
