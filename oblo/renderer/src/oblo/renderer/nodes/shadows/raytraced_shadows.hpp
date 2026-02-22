#pragma once

#include <oblo/math/vec2.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/data/raytraced_shadow_config.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>

namespace oblo
{
    struct raytraced_shadows
    {
        pin::data<vec2u> inResolution;
        pin::data<raytraced_shadow_config> inConfig;

        pin::buffer inCameraBuffer;
        pin::buffer inLightBuffer;

        pin::texture inDepthBuffer;
        pin::texture outShadow;

        h32<raytracing_pass> shadowPass;
        h32<raytracing_pass_instance> shadowPassInstance;

        u32 randomSeed;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}