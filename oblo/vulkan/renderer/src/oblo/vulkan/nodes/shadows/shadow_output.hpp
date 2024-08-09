#pragma once

#include <oblo/vulkan/data/light_visibility_event.hpp>
#include <oblo/vulkan/data/raytraced_shadow_config.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo::vk
{
    struct shadow_output
    {
        data<raytraced_shadow_config> inConfig;

        resource<texture> outShadow;
        data_sink<light_visibility_event> outShadowSink;

        void build(const frame_graph_build_context& builder);
    };
}