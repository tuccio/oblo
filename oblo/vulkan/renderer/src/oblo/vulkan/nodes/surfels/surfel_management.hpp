#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/aabb.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/instance_table_node.hpp>

namespace oblo::vk
{
    /// @brief Creates the buffers necessary for surfels GI.
    struct surfel_initializer
    {
        resource<buffer> outSurfelsStack;
        resource<buffer> outSurfelsPool;

        resource<buffer> outSurfelsGrid;

        resource<texture> inVisibilityBuffer;

        data<u32> inMaxSurfels;
        data<aabb> inGridBounds;
        data<vec3> inGridCellSize;

        h32<compute_pass> initStackPass;
        h32<compute_pass> initGridPass;

        bool structuresInitialized;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    /// @brief Screen-space pass that calculates coverage of each 16x16 tile and the best candidate pixel within the
    /// tile to spawn a surfel on.
    struct surfel_tiling
    {
        resource<buffer> outTileCoverage;

        resource<buffer> inSurfelsGrid;

        resource<buffer> inCameraBuffer;

        h32<compute_pass> tilingPass;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    /// @brief Goes over the tile coverage, determines whether or not to spawn a surfel in each tile.
    struct surfel_spawner
    {
        resource<buffer> inTileCoverage;

        resource<buffer> inSurfelsStack;
        resource<buffer> inSurfelsPool;

        resource<buffer> inOutSurfelsGrid;

        h32<compute_pass> spawnPass;

        u32 randomSeed;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}