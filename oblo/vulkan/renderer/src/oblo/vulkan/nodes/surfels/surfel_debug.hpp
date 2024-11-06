#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <span>

namespace oblo::vk
{
    struct surfel_debug
    {
        resource<buffer> inCameraBuffer;

        resource<texture> inDepthBuffer;

        resource<texture> inOutImage;

        resource<buffer> inSurfelsPool;
        resource<buffer> inSurfelsGrid;

        h32<render_pass> debugPass;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}