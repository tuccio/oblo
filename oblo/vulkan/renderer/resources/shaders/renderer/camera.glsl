#ifndef OBLO_INCLUDE_RENDERER_CAMERA
#define OBLO_INCLUDE_RENDERER_CAMERA

#include <renderer/volumes>

struct camera_buffer
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    frustum frustum;
};

bool camera_depth_no_hit(float depth)
{
    return depth == 0.f;
}

#endif