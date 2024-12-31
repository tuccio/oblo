#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/aabb.hpp>
#include <oblo/math/vec2u.hpp>
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

        data<u32> inMaxSurfels;
        data<aabb> inGridBounds;
        data<vec3> inGridCellSize;

        h32<compute_pass> initStackPass;

        bool stackInitialized;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    struct surfel_tiling_data
    {
        resource<buffer> buffer;
    };

    /// @brief Screen-space pass that calculates coverage of each 16x16 tile and the best candidate pixel within the
    /// tile to spawn a surfel on.
    struct surfel_tiling
    {
        data_sink<surfel_tiling_data> outTileCoverageSink;

        resource<buffer> inSurfelsGrid;
        resource<buffer> inSurfelsPool;

        resource<buffer> inCameraBuffer;
        resource<texture> inVisibilityBuffer;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<buffer> inMeshDatabase;

        h32<compute_pass> tilingPass;
        h32<compute_pass> reductionPass;

        u32 reductionGroupSize;

        struct subpass_info;
        std::span<subpass_info> subpasses;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    /// @brief Goes over the tile coverage, determines whether or not to spawn a surfel in each tile.
    struct surfel_spawner
    {
        data_sink<surfel_tiling_data> inTileCoverageSink;

        resource<buffer> inOutSurfelsStack;
        resource<buffer> inOutSurfelsPool;

        resource<buffer> inOutSurfelsGrid;

        h32<compute_pass> spawnPass;

        u32 randomSeed;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    /// @brief Once a frame, clear the grid before refilling it.
    struct surfel_grid_clear
    {
        resource<buffer> inOutSurfelsGrid;

        data<aabb> inGridBounds;
        data<vec3> inGridCellSize;

        h32<compute_pass> initGridPass;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    /// @brief Fills the grid from the currently alive surfels.
    struct surfel_update
    {
        resource<buffer> inOutSurfelsPool;
        resource<buffer> inOutSurfelsGrid;

        data<u32> inMaxSurfels;

        h32<compute_pass> updatePass;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}