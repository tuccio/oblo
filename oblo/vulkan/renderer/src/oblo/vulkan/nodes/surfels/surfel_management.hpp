#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/aabb.hpp>
#include <oblo/math/vec3u.hpp>
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
        resource<buffer> outSurfelsGridData;

        // Two buffers we ping pong during the ray-tracing update
        resource<buffer> outSurfelsLightingData0;
        resource<buffer> outSurfelsLightingData1;
        resource<buffer> outSurfelsLightEstimatorData;
        resource<buffer> outSurfelsLastUsage;

        resource<buffer> outLastFrameSurfelsLightingData;
        resource<buffer> outSurfelsLightingData;

        data<u32> inMaxSurfels;
        data<aabb> inGridBounds;
        data<f32> inGridCellSize;

        data<vec3u> outCellsCount;

        h32<compute_pass> initStackPass;
        h32<compute_pass_instance> initPassInstance;

        u8 outputSelector;

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
        resource<buffer> inSurfelsGridData;
        resource<buffer> inSurfelsData;
        resource<buffer> inSurfelsSpawnData; // This is not used, it's just here to forward it to debug views
        resource<buffer> inLastFrameSurfelsLightingData;

        data<camera_buffer> inCameraData;
        resource<buffer> inCameraBuffer;
        resource<texture> inVisibilityBuffer;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<buffer> inMeshDatabase;

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
        data_sink<surfel_tiling_data> inTileCoverageSink;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<buffer> inOutSurfelsStack;
        resource<buffer> inOutSurfelsSpawnData;
        resource<buffer> inOutSurfelsData;
        resource<buffer> inOutSurfelsLastUsage;
        resource<buffer> inOutLastFrameSurfelsLightingData;

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
        resource<buffer> inOutSurfelsSpawnData;
        resource<buffer> inOutSurfelsStack;
        resource<buffer> inOutSurfelsGrid;
        resource<buffer> inOutSurfelsGridData;
        resource<buffer> inOutSurfelsLastUsage;
        resource<buffer> outGridFillBuffer;

        resource<buffer> inOutSurfelsData;
        resource<buffer> inSurfelsLightEstimatorData;

        resource<buffer> inMeshDatabase;
        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<buffer> inEntitySetBuffer;

        data<aabb> inGridBounds;
        data<f32> inGridCellSize;
        data<vec3u> inCellsCount;
        data<u32> inMaxSurfels;

        data_sink<camera_buffer> inCameras;

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
        data<u32> inMaxSurfels;

        data<resource<buffer>> outTotalRayCount;

        resource<buffer> inSurfelsData;

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
        data<u32> inMaxRayPaths;
        data<u32> inMaxSurfels;
        data<f32> inGIMultiplier;

        resource<buffer> inOutSurfelsGrid;
        resource<buffer> inOutSurfelsData;
        resource<buffer> inOutSurfelsGridData;
        resource<buffer> inOutSurfelsLastUsage;

        resource<buffer> inOutSurfelsLightEstimatorData;

        resource<buffer> inLastFrameSurfelsLightingData;
        resource<buffer> inOutSurfelsLightingData;

        resource<buffer> inLightBuffer;
        resource<buffer> inLightConfig;

        data<resource<buffer>> inTotalRayCount;

        resource<buffer> inSkyboxSettingsBuffer;

        resource<buffer> inMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        h32<raytracing_pass> rtPass;
        h32<raytracing_pass_instance> rtPassInstance;

        u32 randomSeed;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}