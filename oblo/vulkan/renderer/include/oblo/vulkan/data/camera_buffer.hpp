#pragma once

#include <oblo/math/frustum.hpp>
#include <oblo/math/mat4.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo::vk
{
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
        vec3 lastFramePosition;
        float far;
    };
}