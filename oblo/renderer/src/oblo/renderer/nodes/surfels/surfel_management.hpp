#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/aabb.hpp>
#include <oblo/math/vec3u.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>

namespace oblo
{
    struct camera_buffer;

    /// @brief Creates the buffers necessary for surfels GI.
    struct surfel_initializer
    {
        pin::buffer outSurfelsStack;
        pin::buffer outSurfelsSpawnData;
        pin::buffer outSurfelsData;

        pin::buffer outSurfelsGrid;
        pin::buffer outSurfelsGridData;

        // Two buffers we ping pong during the ray-tracing update
        pin::buffer outSurfelsLightingData0;
        pin::buffer outSurfelsLightingData1;
        pin::buffer outSurfelsLightEstimatorData;
        pin::buffer outSurfelsLastUsage;

        pin::buffer outLastFrameSurfelsLightingData;
        pin::buffer outSurfelsLightingData;

        pin::buffer outSurfelsMetrics;

        pin::data<u32> inMaxSurfels;
        pin::data<aabb> inGridBounds;
        pin::data<f32> inGridCellSize;

        pin::data<vec3u> outCellsCount;

        h32<compute_pass> initStackPass;
        h32<compute_pass_instance> initPassInstance;

        u8 outputSelector;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    struct surfel_tiling_data
    {
        pin::buffer buffer;
        u32 elements;
    };

    /// @brief Screen-space pass that calculates coverage of each 16x16 tile and the best candidate pixel within the
    /// tile to spawn a surfel on.
    struct surfel_tiling
    {
        pin::data_sink<surfel_tiling_data> outTileCoverageSink;
        pin::data_sink<vec3> outCameraPositionSink;

        pin::buffer outFullTileCoverage;

        pin::buffer inSurfelsGrid;
        pin::buffer inSurfelsGridData;
        pin::buffer inSurfelsData;
        pin::buffer inSurfelsSpawnData; // This is not used, it's just here to forward it to debug views
        pin::buffer inLastFrameSurfelsLightingData;

        pin::data<camera_buffer> inCameraData;
        pin::buffer inCameraBuffer;
        pin::texture inVisibilityBuffer;

        pin::buffer inInstanceTables;
        pin::data<instance_data_table_buffers_span> inInstanceBuffers;

        pin::buffer inMeshDatabase;

        h32<compute_pass> tilingPass;
        h32<compute_pass_instance> tilingPassInstance;

        u32 randomSeed;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    /// @brief Goes over the tile coverage, determines whether or not to spawn a surfel in each tile.
    struct surfel_spawner
    {
        pin::data_sink<surfel_tiling_data> inTileCoverageSink;

        pin::buffer inInstanceTables;
        pin::data<instance_data_table_buffers_span> inInstanceBuffers;

        pin::buffer inOutSurfelsStack;
        pin::buffer inOutSurfelsSpawnData;
        pin::buffer inOutSurfelsData;
        pin::buffer inOutSurfelsLastUsage;
        pin::buffer inOutLastFrameSurfelsLightingData;

        h32<compute_pass> spawnPass;
        h32<compute_pass_instance> spawnPassInstance;

        u32 randomSeed;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    /// @brief Fills the grid from the currently alive surfels, updates surfels that moved, and frees unused one.
    struct surfel_update
    {
        pin::buffer inSurfelsMetrics;

        pin::buffer inOutSurfelsSpawnData;
        pin::buffer inOutSurfelsStack;
        pin::buffer inOutSurfelsGrid;
        pin::buffer inOutSurfelsGridData;
        pin::buffer inOutSurfelsLastUsage;
        pin::buffer outGridFillBuffer;

        pin::buffer inOutSurfelsData;
        pin::buffer inSurfelsLightEstimatorData;

        pin::buffer inMeshDatabase;
        pin::buffer inInstanceTables;
        pin::data<instance_data_table_buffers_span> inInstanceBuffers;

        pin::buffer inEntitySetBuffer;

        pin::data<aabb> inGridBounds;
        pin::data<f32> inGridCellSize;
        pin::data<vec3u> inCellsCount;
        pin::data<u32> inMaxSurfels;

        pin::data_sink<camera_buffer> inCameras;

        h32<compute_pass> overcoveragePass;
        h32<compute_pass> clearPass;
        h32<compute_pass> updatePass;
        h32<compute_pass> allocatePass;
        h32<compute_pass> fillPass;

        h32<compute_pass_instance> overcoverageFgPass;
        h32<compute_pass_instance> clearFgPass;
        h32<compute_pass_instance> updateFgPass;
        h32<compute_pass_instance> allocateFgPass;
        h32<compute_pass_instance> fillFgPass;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    struct surfel_accumulate_raycount
    {
        pin::data<u32> inMaxSurfels;

        pin::data<pin::buffer> outTotalRayCount;

        pin::buffer inSurfelsData;

        h32<compute_pass> reducePass;

        struct subpass_info;

        std::span<subpass_info> subpasses;

        u32 reductionGroupSize;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    struct surfel_raytracing
    {
        pin::data<u32> inMaxRayPaths;
        pin::data<u32> inMaxSurfels;
        pin::data<f32> inGIMultiplier;

        pin::buffer inSurfelsMetrics;

        pin::buffer inOutSurfelsGrid;
        pin::buffer inOutSurfelsData;
        pin::buffer inOutSurfelsGridData;
        pin::buffer inOutSurfelsLastUsage;

        pin::buffer inOutSurfelsLightEstimatorData;

        pin::buffer inLastFrameSurfelsLightingData;
        pin::buffer inOutSurfelsLightingData;

        pin::buffer inLightBuffer;
        pin::buffer inLightConfig;

        pin::data<pin::buffer> inTotalRayCount;

        pin::buffer inSkyboxSettingsBuffer;

        pin::buffer inMeshDatabase;

        pin::buffer inInstanceTables;
        pin::data<instance_data_table_buffers_span> inInstanceBuffers;

        h32<raytracing_pass> rtPass;
        h32<raytracing_pass_instance> rtPassInstance;

        u32 randomSeed;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}