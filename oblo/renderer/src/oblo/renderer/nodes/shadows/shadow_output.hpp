#pragma once

#include <oblo/renderer/data/light_visibility_event.hpp>
#include <oblo/renderer/data/raytraced_shadow_config.hpp>
#include <oblo/renderer/graph/pins.hpp>

namespace oblo
{
    struct shadow_output
    {
        pin::data<raytraced_shadow_config> inConfig;

        pin::texture outShadow;
        data_sink<light_visibility_event> outShadowSink;

        void build(const frame_graph_build_context& builder);
    };
}