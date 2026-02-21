#pragma once

#include <oblo/renderer/draw/draw_registry.hpp>
#include <oblo/renderer/graph/pins.hpp>

namespace oblo
{
    struct draw_buffer_data
    {
        pin::buffer drawCallCountBuffer;
        pin::buffer preCullingIdMap;
        batch_draw_data sourceData;
    };
}