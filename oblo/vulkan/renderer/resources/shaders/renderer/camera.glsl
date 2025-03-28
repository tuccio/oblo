#ifndef OBLO_INCLUDE_RENDERER_CAMERA
#define OBLO_INCLUDE_RENDERER_CAMERA

#include <renderer/geometry/volumes>

struct camera_buffer
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;
    mat4 invProjection;
    mat4 lastFrameViewProjection;
    frustum frustum;
    vec3 position;
    float near;
    vec3 _padding;
    float far;
};

bool camera_depth_no_hit(float depth)
{
    return depth == 0.f;
}

float camera_linearize_depth_ndc(in camera_buffer camera, in float depth)
{
    // Equivalent to:
    //   vec4 h = camera.invProjection * vec4(0, 0, depth, 1);
    //   return h.z / h.w;
    return -camera.near * camera.far / (camera.near - depth * (camera.far - camera.near));
}

vec3 camera_unproject_world_space(in camera_buffer camera, in vec2 positionNDC, in float depth)
{
    vec4 h = camera.invViewProjection * vec4(positionNDC.xy, depth, 1);
    return vec3(h.xyz / h.w);
}

vec2 screen_to_ndc(in uvec2 screenPos, in uvec2 resolution)
{
    return vec2(2 * screenPos + .5f) / resolution - 1.f;
}

vec3 camera_ray_direction(in camera_buffer camera, in vec2 positionNDC)
{
    const vec3 screenPosWS = camera_unproject_world_space(camera, positionNDC, 0);
    return normalize(screenPosWS - camera.position);
}

#endif