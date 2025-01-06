#pragma once

#include <oblo/core/dynamic_array.hpp>
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

        resource<buffer> inSurfelsData;
        resource<buffer> inSurfelsGrid;

        h32<render_pass> debugPass;

        resource<buffer> sphereGeometry;
        dynamic_array<f32> sphereGeometryData;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    struct surfel_debug_tile_coverage
    {
        resource<buffer> inTileCoverage;
        resource<texture> outImage;

        h32<compute_pass> debugPass;

        data<vec2u> inResolution;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}