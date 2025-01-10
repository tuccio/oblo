#ifndef OBLO_INCLUDE_RENDERER_SKYBOX_SKYBOX
#define OBLO_INCLUDE_RENDERER_SKYBOX_SKYBOX

#include <renderer/constants>
#include <renderer/math>

vec2 skybox_uv_from_ray_direction(in vec3 direction)
{
    vec2 uv;
    uv.x = 0.5f + atan(direction.z, direction.x) / (2 * float_pi());
    uv.y = 0.5f - asin(direction.y) / float_pi();
    return uv;
}

#endif