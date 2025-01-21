#include <oblo/vulkan/nodes/surfels/surfel_management.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/random_generator.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/vulkan/data/camera_buffer.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/draw/raytracing_pass_initializer.hpp>
#include <oblo/vulkan/events/gi_reset_event.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    namespace
    {
        constexpr u32 g_tileSize{32};

        // A single surfel might be inserted in a few neighboring cells in the grid, if it's big enough
        // We are overestimating here, since the radius is limited to the grid cell size
        constexpr u32 g_MaxSurfelMultiplicity = 27;

        struct surfel_spawn_data
        {
            ecs::entity entity;
            // TODO (#62): We should handle the mesh changing on the entity (it may invalidate the meshlet id)
            u32 packedMeshletAndTriangleId;
            f32 barycentricU;
            f32 barycentricV;
            u32 spawnTimestamp;
        };

        struct surfel_dynamic_data
        {
            vec3 position;
            f32 radius;
            vec3 normal;
            u32 requestedRays;
        };

        struct surfel_grid_header
        {
            vec3 boundsMin;
            f32 cellSize;
            vec3 boundsMax;
            u32 maxSurfels;
            u32 cellsCountX;
            u32 cellsCountY;
            u32 cellsCountZ;
            u32 currentTimestamp;
        };

        struct surfel_grid_cell
        {
            u32 surfelsCount;
            u32 surfelDataBegin;
        };

        struct surfel_stack_header
        {
            u32 available;
        };

        struct surfel_tile_data
        {
            f32 worstPixelCoverage;
            f32 irradianceInit[3];
            surfel_spawn_data spawnData;
        };

        struct surfel_lighting_data
        {
            vec3 irradiance;
            f32 confidence;
        };

        struct surfel_light_estimator_data
        {
            vec3 shortTermMean;
            f32 varianceBasedBlendReduction;
            vec3 variance;
            f32 inconsistency;
        };

        vec3 calculate_centroid(std::span<const camera_buffer> cameras)
        {
            vec3 c{};

            for (const auto& camera : cameras)
            {
                c += camera.position;
            }

            return c / f32(cameras.size());
        }
    }

    void surfel_initializer::init(const frame_graph_init_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        initStackPass = pm.register_compute_pass({
            .name = "Initialize Surfels Pool",
            .shaderSourcePath = "./vulkan/shaders/surfels/initialize_stack.comp",
        });

        OBLO_ASSERT(initStackPass);

        outputSelector = 0;
    }

    void surfel_initializer::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::compute);

        const auto gridBounds = ctx.access(inGridBounds);
        const auto gridCellSize = ctx.access(inGridCellSize);
        const auto maxSurfels = ctx.access(inMaxSurfels);

        const auto cellsCountF32 = (gridBounds.max - gridBounds.min) / gridCellSize;
        const auto cellsCount = vec3u{
            .x = u32(std::ceil(cellsCountF32.x)),
            .y = u32(std::ceil(cellsCountF32.y)),
            .z = u32(std::ceil(cellsCountF32.z)),
        };

        ctx.access(outCellsCount) = cellsCount;

        const auto cellsCountLinearized = cellsCount.x * cellsCount.y * cellsCount.z;

        const u32 surfelsStackSize = sizeof(u32) * (maxSurfels + 1);
        const u32 SurfelsSpawnDataSize = sizeof(surfel_spawn_data) * maxSurfels;
        const u32 surfelsDataSize = sizeof(surfel_dynamic_data) * maxSurfels;
        const u32 surfelsLightingDataSize = sizeof(surfel_lighting_data) * maxSurfels;
        const u32 surfelsLightEstimatorgDataSize = sizeof(surfel_light_estimator_data) * maxSurfels;
        const u32 surfelsGridSize = u32(sizeof(surfel_grid_header) + sizeof(surfel_grid_cell) * cellsCountLinearized);
        const u32 surfelsGridDataSize = u32((1 + g_MaxSurfelMultiplicity * maxSurfels) * sizeof(u32));
        const u32 surfelsLastUsageBufferSize = u32((maxSurfels) * sizeof(u32));

        // TODO: After creation and initialization happened, the usage could be none to avoid any useless memory barrier
        ctx.create(outSurfelsStack,
            buffer_resource_initializer{
                .size = surfelsStackSize,
                .isStable = true,
            },
            buffer_usage::storage_write);

        ctx.create(outSurfelsSpawnData,
            buffer_resource_initializer{
                .size = SurfelsSpawnDataSize,
                .isStable = true,
            },
            buffer_usage::storage_write);

        ctx.create(outSurfelsData,
            buffer_resource_initializer{
                .size = surfelsDataSize,
                .isStable = true,
            },
            buffer_usage::storage_write);

        ctx.create(outSurfelsLastUsage,
            buffer_resource_initializer{
                .size = surfelsLastUsageBufferSize,
                .isStable = true,
            },
            buffer_usage::storage_write);

        // Probably only one of the two really needs to be stable at a given time
        ctx.create(outSurfelsLightingData0,
            buffer_resource_initializer{
                .size = surfelsLightingDataSize,
                .isStable = true,
            },
            buffer_usage::storage_write);

        ctx.create(outSurfelsLightingData1,
            buffer_resource_initializer{
                .size = surfelsLightingDataSize,
                .isStable = true,
            },
            buffer_usage::storage_write);

        ctx.create(outSurfelsLightEstimatorData,
            buffer_resource_initializer{
                .size = surfelsLightEstimatorgDataSize,
                .isStable = true,
            },
            buffer_usage::storage_write);

        // The greed has to be stable because it goes out as last frame output from here
        ctx.create(outSurfelsGrid,
            buffer_resource_initializer{
                .size = surfelsGridSize,
                .isStable = true,
            },
            buffer_usage::storage_write);

        ctx.create(outSurfelsGridData,
            buffer_resource_initializer{
                .size = surfelsGridDataSize,
                .isStable = true,
            },
            buffer_usage::storage_write);

        // Ping-ping the light buffers: last frame output becomes the input
        const auto inputSelector = outputSelector;
        outputSelector = 1 - outputSelector;

        const resource<buffer> lightingDataBuffers[] = {
            outSurfelsLightingData0,
            outSurfelsLightingData1,
        };

        ctx.reroute(lightingDataBuffers[inputSelector], outLastFrameSurfelsLightingData);
        ctx.reroute(lightingDataBuffers[outputSelector], outSurfelsLightingData);
    }

    void surfel_initializer::execute(const frame_graph_execute_context& ctx)
    {
        const bool mustReinitialize = ctx.has_event<gi_reset_event>() ||
            ctx.get_frames_alive_count(outSurfelsStack) == 0 || ctx.get_frames_alive_count(outSurfelsSpawnData) == 0 ||
            ctx.get_frames_alive_count(outSurfelsData) == 0 ||
            ctx.get_frames_alive_count(outSurfelsLightingData0) == 0 ||
            ctx.get_frames_alive_count(outSurfelsLightingData1) == 0;

        // Initialize the grid every frame, we fill it after updating/spawning
        auto& pm = ctx.get_pass_manager();

        // We only need to initialize the stack once, but we could also run this code to reset surfels
        if (!mustReinitialize)
        {
            return;
        }

        const auto initStackPipeline = pm.get_or_create_pipeline(initStackPass, {});

        if (const auto pass = pm.begin_compute_pass(ctx.get_command_buffer(), initStackPipeline))
        {
            binding_table bindings;

            ctx.bind_buffers(bindings,
                {
                    {"b_SurfelsStack", outSurfelsStack},
                    {"b_SurfelsSpawnData", outSurfelsSpawnData},
                    {"b_SurfelsData", outSurfelsData},
                    {"b_InSurfelsLighting", outSurfelsLightingData0},
                    {"b_OutSurfelsLighting", outSurfelsLightingData1},
                    {"b_OutSurfelsLightEstimator", outSurfelsLightEstimatorData},
                });

            const binding_table* bindingTables[] = {
                &bindings,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            const auto subgroupSize = pm.get_subgroup_size();

            const u32 maxSurfels = ctx.access(inMaxSurfels);
            const auto groups = round_up_div(maxSurfels, subgroupSize);

            pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span(&maxSurfels, 1)));

            vkCmdDispatch(ctx.get_command_buffer(), groups, 1, 1);

            pm.end_compute_pass(*pass);
        }
    }

    void surfel_tiling::init(const frame_graph_init_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        tilingPass = pm.register_compute_pass({
            .name = "Surfel Tiling",
            .shaderSourcePath = "./vulkan/shaders/surfels/tiling.comp",
        });

        OBLO_ASSERT(tilingPass);
    }

    void surfel_tiling::build(const frame_graph_build_context& ctx)
    {
        randomSeed = ctx.get_random_generator().generate();

        const auto resolution =
            ctx.get_current_initializer(inVisibilityBuffer).assert_value_or(image_initializer{}).extent;

        const u32 tilesX = round_up_div(resolution.width, g_tileSize);
        const u32 tilesY = round_up_div(resolution.height, g_tileSize);
        const u32 tilesCount = tilesX * tilesY;

        const u32 tilesBufferSize = u32(tilesCount * sizeof(surfel_tile_data));
        u32 currentBufferSize = tilesBufferSize;

        ctx.begin_pass(pass_kind::compute);

        ctx.create(outFullTileCoverage,
            buffer_resource_initializer{
                .size = tilesBufferSize,
            },
            buffer_usage::storage_write);

        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

        if (ctx.has_source(inSurfelsGrid))
        {
            ctx.acquire(inSurfelsGrid, buffer_usage::storage_read);
            ctx.acquire(inSurfelsData, buffer_usage::storage_read);
            ctx.acquire(inSurfelsGridData, buffer_usage::storage_read);
            ctx.acquire(inOutSurfelsLastUsage, buffer_usage::storage_write);
            ctx.acquire(inLastFrameSurfelsLightingData, buffer_usage::storage_read);
        }

        ctx.push(outTileCoverageSink,
            {
                .buffer = outFullTileCoverage,
                .elements = currentBufferSize / u32{sizeof(surfel_tile_data)},
            });

        const vec3 cameraPosition = ctx.access(inCameraData).position;
        ctx.push(outCameraPositionSink, cameraPosition);
    }

    void surfel_tiling::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        string_builder sb;
        sb.format("TILE_SIZE {}", g_tileSize);

        const hashed_string_view defines[] = {sb.as<hashed_string_view>()};

        binding_table bindingTable;

        const binding_table* bindingTables[] = {
            &bindingTable,
        };

        const auto commandBuffer = ctx.get_command_buffer();

        const auto tilingPipeline = pm.get_or_create_pipeline(tilingPass,
            {
                .defines = {defines},
            });

        const auto resolution = ctx.access(inVisibilityBuffer).initializer.extent;

        const u32 tilesX = round_up_div(resolution.width, g_tileSize);
        const u32 tilesY = round_up_div(resolution.height, g_tileSize);

        if (const auto tiling = pm.begin_compute_pass(commandBuffer, tilingPipeline))
        {
            ctx.bind_buffers(bindingTable,
                {
                    {"b_InstanceTables", inInstanceTables},
                    {"b_MeshTables", inMeshDatabase},
                    {"b_CameraBuffer", inCameraBuffer},
                    {"b_SurfelsGrid", inSurfelsGrid},
                    {"b_SurfelsGridData", inSurfelsGridData},
                    {"b_SurfelsData", inSurfelsData},
                    {"b_OutTileCoverage", outFullTileCoverage},
                    {"b_SurfelsLastUsage", inOutSurfelsLastUsage},
                    {"b_InSurfelsLighting", inLastFrameSurfelsLightingData},
                });

            ctx.bind_textures(bindingTable,
                {
                    {"t_InVisibilityBuffer", inVisibilityBuffer},
                });

            pm.bind_descriptor_sets(*tiling, bindingTables);
            pm.push_constants(*tiling, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&randomSeed, 1}));

            vkCmdDispatch(ctx.get_command_buffer(), tilesX, tilesY, 1);

            pm.end_compute_pass(*tiling);
        }
    }

    void surfel_spawner::init(const frame_graph_init_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        spawnPass = pm.register_compute_pass({
            .name = "Surfel Spawn",
            .shaderSourcePath = "./vulkan/shaders/surfels/spawn.comp",
        });

        OBLO_ASSERT(spawnPass);
    }

    void surfel_spawner::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::compute);

        const auto tileCoverageSpan = ctx.access(inTileCoverageSink);

        if (tileCoverageSpan.empty())
        {
            return;
        }

        for (const auto& tileCoverage : tileCoverageSpan)
        {
            ctx.acquire(tileCoverage.buffer, buffer_usage::storage_read);
        }

        ctx.acquire(inOutSurfelsSpawnData, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsData, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsStack, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsLastUsage, buffer_usage::storage_write);
        ctx.acquire(inOutLastFrameSurfelsLightingData, buffer_usage::storage_write);

        randomSeed = ctx.get_random_generator().generate();
    }

    void surfel_spawner::execute(const frame_graph_execute_context& ctx)
    {
        const auto tileCoverageSpan = ctx.access(inTileCoverageSink);

        if (tileCoverageSpan.empty())
        {
            return;
        }

        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;
        binding_table perDispatchBindingTable;

        ctx.bind_buffers(bindingTable,
            {
                {"b_SurfelsSpawnData", inOutSurfelsSpawnData},
                {"b_SurfelsData", inOutSurfelsData},
                {"b_SurfelsStack", inOutSurfelsStack},
                {"b_SurfelsLastUsage", inOutSurfelsLastUsage},
                {"b_InSurfelsLighting", inOutLastFrameSurfelsLightingData},
            });

        const auto commandBuffer = ctx.get_command_buffer();

        const auto pipeline = pm.get_or_create_pipeline(spawnPass, {});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
        {
            for (const auto& tileCoverage : tileCoverageSpan)
            {
                perDispatchBindingTable.clear();

                ctx.bind_buffers(perDispatchBindingTable,
                    {
                        {"b_TileCoverage", tileCoverage.buffer},
                    });

                const binding_table* bindingTables[] = {
                    &bindingTable,
                    &perDispatchBindingTable,
                };

                pm.bind_descriptor_sets(*pass, bindingTables);

                struct push_constants
                {
                    u32 srcElements;
                    u32 randomSeed;
                    u32 currentTimestamp;
                };

                const push_constants constants{
                    .srcElements = tileCoverage.elements,
                    .randomSeed = randomSeed,
                    .currentTimestamp = ctx.get_current_frames_count(),
                };

                pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&constants, 1}));

                const u32 groups = round_up_div(tileCoverage.elements, pm.get_subgroup_size());

                vkCmdDispatch(ctx.get_command_buffer(), groups, 1, 1);
            }

            pm.end_compute_pass(*pass);
        }
    }

    void surfel_grid_clear::init(const frame_graph_init_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        initGridPass = pm.register_compute_pass({
            .name = "Clear Surfels Grid",
            .shaderSourcePath = "./vulkan/shaders/surfels/clear_grid.comp",
        });

        OBLO_ASSERT(initGridPass);
    }

    void surfel_grid_clear::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::compute);
        ctx.acquire(inOutSurfelsGrid, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsGridData, buffer_usage::storage_write);

        const auto cellsCount = ctx.access(inCellsCount);

        const auto cellsCountLinearized = cellsCount.x * cellsCount.y * cellsCount.z;

        ctx.create(outGridFillBuffer,
            buffer_resource_initializer{
                .size = u32(sizeof(u32) * cellsCountLinearized),
                .isStable = true, // It doesn't need to be stable, but this might exceed the chunk size of the monotonic
                                  // allocator
            },
            buffer_usage::storage_write);

        const auto centroid = calculate_centroid(ctx.access(inCameras));
        ctx.access(outCameraCentroid) = centroid;
    }

    void surfel_grid_clear::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();
        const auto initGridPipeline = pm.get_or_create_pipeline(initGridPass, {});

        if (const auto pass = pm.begin_compute_pass(ctx.get_command_buffer(), initGridPipeline))
        {
            binding_table bindings;

            ctx.bind_buffers(bindings,
                {
                    {"b_SurfelsGrid", inOutSurfelsGrid},
                    {"b_SurfelsGridData", inOutSurfelsGridData},
                    {"b_SurfelsGridFill", outGridFillBuffer},
                });

            const binding_table* bindingTables[] = {
                &bindings,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            const auto gridBounds = ctx.access(inGridBounds);
            const auto gridCellSize = ctx.access(inGridCellSize);
            const auto cellsCount = ctx.access(inCellsCount);

            const u32 cellsCountLinearized = cellsCount.x * cellsCount.y * cellsCount.z;

            const auto centroid = ctx.access(outCameraCentroid);

            const surfel_grid_header header{
                .boundsMin = gridBounds.min + centroid,
                .cellSize = gridCellSize,
                .boundsMax = gridBounds.max + centroid,
                .maxSurfels = ctx.access(inMaxSurfels),
                .cellsCountX = cellsCount.x,
                .cellsCountY = cellsCount.y,
                .cellsCountZ = cellsCount.z,
                .currentTimestamp = ctx.get_current_frames_count(),
            };

            const auto subgroupSize = pm.get_subgroup_size();

            const auto groupsX = round_up_div(cellsCountLinearized, subgroupSize);

            pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span(&header, 1)));

            vkCmdDispatch(ctx.get_command_buffer(), groupsX, 1, 1);

            pm.end_compute_pass(*pass);
        }
    }

    void surfel_update::init(const frame_graph_init_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        updatePass = pm.register_compute_pass({
            .name = "Surfel Update",
            .shaderSourcePath = "./vulkan/shaders/surfels/update.comp",
        });

        allocatePass = pm.register_compute_pass({
            .name = "Surfel Grid Allocate",
            .shaderSourcePath = "./vulkan/shaders/surfels/allocate_grid.comp",
        });

        fillPass = pm.register_compute_pass({
            .name = "Surfel Grid Fill",
            .shaderSourcePath = "./vulkan/shaders/surfels/fill_grid.comp",
        });

        OBLO_ASSERT(updatePass);
        OBLO_ASSERT(allocatePass);
        OBLO_ASSERT(fillPass);
    }

    void surfel_update::build(const frame_graph_build_context& ctx)
    {
        {
            updateFgPass = ctx.begin_pass(pass_kind::compute);

            ctx.acquire(inOutSurfelsGrid, buffer_usage::storage_write);
            ctx.acquire(inOutSurfelsSpawnData, buffer_usage::storage_write);
            ctx.acquire(inOutSurfelsData, buffer_usage::storage_write);
            ctx.acquire(inOutSurfelsLastUsage, buffer_usage::storage_read);

            ctx.acquire(inEntitySetBuffer, buffer_usage::storage_read);

            ctx.acquire(inMeshDatabase, buffer_usage::storage_read);
            acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
        }

        {
            allocateFgPass = ctx.begin_pass(pass_kind::compute);

            ctx.acquire(inOutSurfelsGrid, buffer_usage::storage_write);
            ctx.acquire(inOutSurfelsGridData, buffer_usage::storage_write);
        }

        {
            fillFgPass = ctx.begin_pass(pass_kind::compute);

            ctx.acquire(inOutSurfelsData, buffer_usage::storage_read);
            ctx.acquire(inOutSurfelsGrid, buffer_usage::storage_read);
            ctx.acquire(inOutSurfelsGridData, buffer_usage::storage_write);
            ctx.acquire(inSurfelsLightEstimatorData, buffer_usage::storage_read);
            ctx.acquire(inGridFillBuffer, buffer_usage::storage_write);
        }
    }

    void surfel_update::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_buffers(bindingTable,
            {
                {"b_SurfelsGrid", inOutSurfelsGrid},
                {"b_SurfelsGridData", inOutSurfelsGridData},
                {"b_SurfelsGridFill", inGridFillBuffer},
                {"b_SurfelsSpawnData", inOutSurfelsSpawnData},
                {"b_SurfelsData", inOutSurfelsData},
                {"b_SurfelsLastUsage", inOutSurfelsLastUsage},
                {"b_SurfelsLightEstimator", inSurfelsLightEstimatorData},
                {"b_SurfelsStack", inOutSurfelsStack},
                {"b_InstanceTables", inInstanceTables},
                {"b_MeshTables", inMeshDatabase},
                {"b_EcsEntitySet", inEntitySetBuffer},
            });

        const auto commandBuffer = ctx.get_command_buffer();

        {
            ctx.begin_pass(updateFgPass);

            const auto pipeline = pm.get_or_create_pipeline(updatePass, {});

            if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
            {
                const auto subgroupSize = pm.get_subgroup_size();

                struct push_constants
                {
                    vec3 cameraCentroid;
                    u32 maxSurfels;
                    u32 currentTimestamp;
                };

                const push_constants constants{
                    .cameraCentroid = ctx.access(inCameraCentroid),
                    .maxSurfels = ctx.access(inMaxSurfels),
                    .currentTimestamp = ctx.get_current_frames_count(),
                };

                const binding_table* bindingTables[] = {
                    &bindingTable,
                };

                pm.bind_descriptor_sets(*pass, bindingTables);
                pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&constants, 1}));

                const u32 groupsX = round_up_div(constants.maxSurfels, subgroupSize);
                vkCmdDispatch(ctx.get_command_buffer(), groupsX, 1, 1);

                pm.end_compute_pass(*pass);
            }
        }

        {
            ctx.begin_pass(allocateFgPass);

            string_builder multiplicityDefine;
            multiplicityDefine.format("SURFEL_MAX_MULTIPLICITY {}", g_MaxSurfelMultiplicity);

            const hashed_string_view defines[] = {multiplicityDefine.as<hashed_string_view>()};

            const auto pipeline = pm.get_or_create_pipeline(allocatePass, {.defines = defines});

            if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
            {
                const auto subgroupSize = pm.get_subgroup_size();

                const binding_table* bindingTables[] = {
                    &bindingTable,
                };

                pm.bind_descriptor_sets(*pass, bindingTables);

                const auto cellsCount = ctx.access(inCellsCount);

                const auto groupsX = round_up_div(cellsCount.x * cellsCount.y * cellsCount.z, subgroupSize);
                vkCmdDispatch(ctx.get_command_buffer(), groupsX, 1, 1);

                pm.end_compute_pass(*pass);
            }
        }

        {
            ctx.begin_pass(fillFgPass);

            const auto pipeline = pm.get_or_create_pipeline(fillPass, {});

            if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
            {
                const auto subgroupSize = pm.get_subgroup_size();

                const binding_table* bindingTables[] = {
                    &bindingTable,
                };

                pm.bind_descriptor_sets(*pass, bindingTables);

                const u32 maxSurfels = ctx.access(inMaxSurfels);
                const u32 groupsX = round_up_div(maxSurfels, subgroupSize);
                vkCmdDispatch(ctx.get_command_buffer(), groupsX, 1, 1);

                pm.end_compute_pass(*pass);
            }
        }
    }

    void surfel_accumulate_raycount::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        reducePass = passManager.register_compute_pass({
            .name = "Surfel Accumulate Ray Count",
            .shaderSourcePath = "./vulkan/shaders/surfels/surfel_raycount.comp",
        });

        const u32 subgroupSize = ctx.get_pass_manager().get_subgroup_size();
        reductionGroupSize = subgroupSize * subgroupSize;
    }

    struct surfel_accumulate_raycount::subpass_info
    {
        h32<frame_graph_pass> id;
        u32 inputSurfels;
        resource<buffer> outputBuffer;
    };

    void surfel_accumulate_raycount::build(const frame_graph_build_context& ctx)
    {
        const u32 maxSurfels = ctx.access(inMaxSurfels);

        const u32 reductionPassesCount = max(1u,
            u32(std::ceilf(f32(log2_round_down_power_of_two(round_up_power_of_two(maxSurfels))) /
                log2_round_down_power_of_two(reductionGroupSize))));

        subpasses = allocate_n_span<subpass_info>(ctx.get_frame_allocator(), reductionPassesCount);

        u32 inputSurfels = maxSurfels;

        for (u32 i = 0; i < reductionPassesCount; ++i)
        {
            const auto subpassId = ctx.begin_pass(pass_kind::compute);

            const auto inputBuffer = i == 0 ? inSurfelsData : subpasses[i - 1].outputBuffer;

            ctx.acquire(inputBuffer, buffer_usage::storage_read);

            const u32 outputSurfels = max(inputSurfels / reductionGroupSize, 1u);
            OBLO_ASSERT(inputSurfels > 1 || i == reductionPassesCount - 1);

            const auto outputBuffer = ctx.create_dynamic_buffer(
                {
                    .size = u32(sizeof(u32) * outputSurfels),
                },
                buffer_usage::storage_write);

            subpasses[i] = {
                .id = subpassId,
                .inputSurfels = inputSurfels,
                .outputBuffer = outputBuffer,
            };

            inputSurfels = outputSurfels;
        }

        OBLO_ASSERT(inputSurfels == 1);

        // Output the last buffer from the node
        ctx.access(outTotalRayCount) = subpasses.back().outputBuffer;
    }

    void surfel_accumulate_raycount::execute(const frame_graph_execute_context& ctx)
    {
        OBLO_ASSERT(!subpasses.empty());

        auto& pm = ctx.get_pass_manager();

        const auto commandBuffer = ctx.get_command_buffer();

        binding_table bindingTable;

        const binding_table* bindingTables[] = {
            &bindingTable,
        };

        const hashed_string_view firstReductionDefines[] = {"READ_SURFELS"_hsv};

        const auto firstReduction = pm.get_or_create_pipeline(reducePass, {.defines = firstReductionDefines});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, firstReduction))
        {
            bindingTable.clear();

            const auto& subpass = subpasses[0];

            ctx.bind_buffers(bindingTable,
                {
                    {"b_SurfelsData", inSurfelsData},
                    {"b_OutBuffer", subpass.outputBuffer},
                });

            pm.bind_descriptor_sets(*pass, bindingTables);

            struct push_constants
            {
                u32 inElements;
            };

            const push_constants constants{
                .inElements = subpass.inputSurfels,
            };

            pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&constants, 1}));

            const u32 groups = round_up_div(constants.inElements, reductionGroupSize);

            vkCmdDispatch(ctx.get_command_buffer(), groups, 1, 1);

            pm.end_compute_pass(*pass);
        }

        for (usize i = 1; i < subpasses.size(); ++i)
        {
            const auto& subpass = subpasses[i];
            ctx.begin_pass(subpass.id);

            const auto reduction = pm.get_or_create_pipeline(reducePass, {});

            if (const auto pass = pm.begin_compute_pass(commandBuffer, reduction))
            {
                bindingTable.clear();

                ctx.bind_buffers(bindingTable,
                    {
                        {"b_InBuffer", subpasses[i - 1].outputBuffer},
                        {"b_OutBuffer", subpass.outputBuffer},
                    });

                pm.bind_descriptor_sets(*pass, bindingTables);

                struct push_constants
                {
                    u32 inElements;
                };

                const push_constants constants{
                    .inElements = subpass.inputSurfels,
                };

                pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&constants, 1}));

                const u32 groups = round_up_div(constants.inElements, reductionGroupSize);

                vkCmdDispatch(ctx.get_command_buffer(), groups, 1, 1);

                pm.end_compute_pass(*pass);
            }
        }
    }

    void surfel_raytracing::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        rtPass = passManager.register_raytracing_pass({
            .name = "Surfel Ray-Tracing",
            .generation = "./vulkan/shaders/surfels/surfel_raygen.rgen",
            .miss =
                {
                    "./vulkan/shaders/surfels/surfel_skybox.rmiss",
                    "./vulkan/shaders/surfels/surfel_shadow.rmiss",
                },
            .hitGroups =
                {
                    {
                        .type = raytracing_hit_type::triangle,
                        .shaders = {"./vulkan/shaders/surfels/surfel_rayhit.rchit"},
                    },
                },
        });
    }

    void surfel_raytracing::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::raytracing);

        ctx.acquire(inOutSurfelsGrid, buffer_usage::storage_read);
        ctx.acquire(inOutSurfelsGridData, buffer_usage::storage_read);
        ctx.acquire(inOutSurfelsData, buffer_usage::storage_read);

        ctx.acquire(inLastFrameSurfelsLightingData, buffer_usage::storage_read);
        ctx.acquire(inOutSurfelsLightingData, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsLightEstimatorData, buffer_usage::storage_write);

        ctx.acquire(inLightConfig, buffer_usage::uniform);
        ctx.acquire(inLightBuffer, buffer_usage::storage_read);

        ctx.acquire(inSkyboxSettingsBuffer, buffer_usage::uniform);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        // Maybe it should be a uniform, but currently we don't have a storage that supports both
        ctx.acquire(ctx.access(inTotalRayCount), buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

        randomSeed = ctx.get_random_generator().generate();
    }

    void surfel_raytracing::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_buffers(bindingTable,
            {
                {"b_MeshTables", inMeshDatabase},
                {"b_InstanceTables", inInstanceTables},
                {"b_LightConfig", inLightConfig},
                {"b_LightData", inLightBuffer},
                {"b_SkyboxSettings", inSkyboxSettingsBuffer},
                {"b_SurfelsGrid", inOutSurfelsGrid},
                {"b_SurfelsGridData", inOutSurfelsGridData},
                {"b_SurfelsData", inOutSurfelsData},
                {"b_InSurfelsLighting", inLastFrameSurfelsLightingData},
                {"b_OutSurfelsLighting", inOutSurfelsLightingData},
                {"b_OutSurfelsLightEstimator", inOutSurfelsLightEstimatorData},
                {"b_TotalRayCount", ctx.access(inTotalRayCount)},
            });

        bindingTable.emplace(ctx.get_string_interner().get_or_add("u_SceneTLAS"),
            make_bindable_object(ctx.get_draw_registry().get_tlas()));

        const auto commandBuffer = ctx.get_command_buffer();

        const auto pipeline = pm.get_or_create_pipeline(rtPass, {.maxPipelineRayRecursionDepth = 2});

        if (const auto pass = pm.begin_raytracing_pass(commandBuffer, pipeline))
        {
            const auto maxSurfels = ctx.access(inMaxSurfels);

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            const struct push_constants
            {
                u32 maxRayPaths;
                u32 randomSeed;
                f32 giMultiplier;
            } constants{
                .maxRayPaths = ctx.access(inMaxRayPaths),
                .randomSeed = randomSeed,
                .giMultiplier = ctx.access(inGIMultiplier),
            };

            pm.bind_descriptor_sets(*pass, bindingTables);
            pm.push_constants(*pass, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, as_bytes(std::span{&constants, 1}));

            pm.trace_rays(*pass, maxSurfels, 1, 1);

            pm.end_raytracing_pass(*pass);
        }
    }
}