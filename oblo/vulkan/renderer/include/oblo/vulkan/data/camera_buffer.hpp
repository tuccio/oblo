#pragma once

#include <oblo/math/frustum.hpp>
#include <oblo/math/mat4.hpp>

namespace oblo::vk
{
    struct camera_buffer
    {
        mat4 view;
        mat4 projection;
        mat4 viewProjection;
        frustum frustum;
    };
}