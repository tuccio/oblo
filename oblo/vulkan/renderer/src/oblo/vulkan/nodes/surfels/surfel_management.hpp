#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/aabb.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

namespace oblo::vk
{
    struct camera_buffer;

    /// @brief Creates the buffers necessary for surfels GI.
    struct surfel_initializer
    {
        resource<buffer> outSurfelsStack;
        resource<buffer> outSurfelsSpawnData;
        resource<buffer> outSurfelsData;

        resource<buffer> outSurfelsGrid;

        data<u32> inMaxSurfels;
        data<aabb> inGridBounds;
        data<f32> inGridCellSize;

        h32<compute_pass> initStackPass;

        bool stackInitialized;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    struct surfel_tiling_data
    {
        resource<buffer> buffer;
        u32 elements;
    };

    /// @brief Screen-space pass that calculates coverage of each 16x16 tile and the best candidate pixel within the
    /// tile to spawn a surfel on.
    struct surfel_tiling
    {
        data_sink<surfel_tiling_data> outTileCoverageSink;
        data_sink<vec3> outCameraPositionSink;

        resource<buffer> outFullTileCoverage;

        resource<buffer> inSurfelsGrid;
        resource<buffer> inSurfelsData;

        data<camera_buffer> inCameraData;
        resource<buffer> inCameraBuffer;
        resource<texture> inVisibilityBuffer;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<buffer> inMeshDatabase;

        h32<compute_pass> tilingPass;
        h32<compute_pass> reductionPass;

        u32 reductionGroupSize;
        u32 randomSeed;

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

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<buffer> inOutSurfelsStack;
        resource<buffer> inOutSurfelsSpawnData;
        resource<buffer> inOutSurfelsData;

        resource<buffer> inOutSurfelsGrid;

        h32<compute_pass> spawnPass;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    /// @brief Once a frame, clear the grid before refilling it.
    struct surfel_grid_clear
    {
        resource<buffer> inOutSurfelsGrid;

        data<aabb> inGridBounds;
        data<f32> inGridCellSize;

        data_sink<camera_buffer> inCameras;
        data<vec3> outCameraCentroid;

        h32<compute_pass> initGridPass;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    /// @brief Fills the grid from the currently alive surfels, updates surfels that moved, and frees unused one.
    struct surfel_update
    {
        resource<buffer> inOutSurfelsSpawnData;
        resource<buffer> inOutSurfelsStack;
        resource<buffer> inOutSurfelsGrid;

        resource<buffer> inOutSurfelsData;

        resource<buffer> inMeshDatabase;
        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<buffer> inEntitySetBuffer;

        data<u32> inMaxSurfels;
        data<vec3> inCameraCentroid;

        h32<compute_pass> updatePass;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}