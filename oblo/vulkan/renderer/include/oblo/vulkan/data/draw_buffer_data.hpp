#pragma once

#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo::vk
{
    struct draw_buffer_data
    {
        resource<buffer> drawCallCountBuffer;
        resource<buffer> preCullingIdMap;
        batch_draw_data sourceData;
    };
}