#ifndef OBLO_INCLUDE_RENDERER_VOLUMES
#define OBLO_INCLUDE_RENDERER_VOLUMES

struct padded_aabb
{
    vec3 min;
    float _padding0;
    vec3 max;
    float _padding1;
};

struct aabb
{
    vec3 min;
    vec3 max;
};

struct plane
{
    vec3 normal;
    float offset;
};

struct frustum
{
    plane planes[6];
};

#endif
