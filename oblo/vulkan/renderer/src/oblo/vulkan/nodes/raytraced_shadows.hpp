#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/raytraced_shadow_config.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo::vk
{
    struct raytraced_shadows
    {
        data<vec2u> inResolution;
        data<raytraced_shadow_config> inConfig;

        resource<buffer> inCameraBuffer;

        resource<texture> inVisibilityBuffer;
        resource<texture> outShadow;

        h32<raytracing_pass> shadowPass;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}