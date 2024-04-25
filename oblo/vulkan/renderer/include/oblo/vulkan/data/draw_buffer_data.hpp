#pragma once

#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>

namespace oblo::vk
{
    struct draw_buffer_data
    {
        resource<buffer> drawCallBuffer;
        batch_draw_data sourceData;
    };
}