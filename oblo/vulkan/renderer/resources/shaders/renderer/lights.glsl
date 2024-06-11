#ifndef OBLO_INCLUDE_RENDERER_LIGHTS
#define OBLO_INCLUDE_RENDERER_LIGHTS

// This has to match the light_type enums in the engine
#define OBLO_LIGHT_TYPE_POINT 0
#define OBLO_LIGHT_TYPE_DIRECTIONAL 1

struct light_data
{
    vec3 position;
    vec3 direction;
    vec3 intensity;
    uint type;
};

struct light_config
{
    uint lightsCount;
};

#endif