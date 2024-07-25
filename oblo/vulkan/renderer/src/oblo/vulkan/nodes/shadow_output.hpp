#pragma once

#include <oblo/vulkan/data/light_visibility_event.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo::vk
{
    struct shadow_output
    {
        resource<texture> outShadow;
        data_sink<light_visibility_event> inOutShadowSink;
    };
}