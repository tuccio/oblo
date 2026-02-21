#pragma once

#include <oblo/math/vec2.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/raytraced_shadow_config.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo
{
    struct raytraced_shadows
    {
        data<vec2u> inResolution;
        data<raytraced_shadow_config> inConfig;

        resource<buffer> inCameraBuffer;
        resource<buffer> inLightBuffer;

        resource<texture> inDepthBuffer;
        resource<texture> outShadow;

        h32<raytracing_pass> shadowPass;
        h32<raytracing_pass_instance> shadowPassInstance;

        u32 randomSeed;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}