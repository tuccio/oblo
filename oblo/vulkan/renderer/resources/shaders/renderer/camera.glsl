#ifndef OBLO_INCLUDE_RENDERER_CAMERA
#define OBLO_INCLUDE_RENDERER_CAMERA

#include <renderer/volumes>

struct camera_buffer
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;
    mat4 invProjection;
    frustum frustum;
    vec3 position;
};

bool camera_depth_no_hit(float depth)
{
    return depth == 0.f;
}

vec3 camera_unproject_world_space(in camera_buffer camera, in vec2 positionNDC, in float depth)
{
    vec4 h = camera.invViewProjection * vec4(positionNDC.xy, depth, 1);
    return vec3(h.xyz / h.w);
}

#endif