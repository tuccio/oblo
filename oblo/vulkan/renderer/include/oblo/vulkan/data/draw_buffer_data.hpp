#pragma once

#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo::vk
{
    struct draw_buffer_data
    {
        resource<buffer> drawInstancesBuffer;
        resource<buffer> drawCallCountBuffer;
        resource<buffer> drawCallBuffer;
        resource<buffer> preCullingIdMap;
        batch_draw_data sourceData;
    };
}