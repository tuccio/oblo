#pragma once

#include <oblo/math/mat4.hpp>

namespace oblo::vk
{
    struct camera_buffer
    {
        mat4 viewProjectionMatrix;
    };
}