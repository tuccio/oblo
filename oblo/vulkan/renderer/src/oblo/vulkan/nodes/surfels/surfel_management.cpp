#include <oblo/vulkan/nodes/surfels/surfel_management.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/random_generator.hpp>
#include <oblo/core/string/string_builder.hpp>
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
        constexpr u32 g_maxSurfelsPerCell{31};

        struct surfel_spawn_data
        {
            ecs::entity entity;
            // TODO (#62): We should handle the mesh changing on the entity (it may invalidate the meshlet id)
            u32 packedMeshletAndTriangleId;
            f32 barycentricU;
            f32 barycentricV;
        };

        struct surfel_dynamic_data
        {
            vec3 position;
            f32 radius;
            vec3 normal;
            u32 globalInstanceId;
        };

        struct surfel_grid_header
        {
            vec3 boundsMin;
            f32 cellSize;
            vec3 boundsMax;
            f32 _padding0;
            u32 cellsCountX;
            u32 cellsCountY;
            u32 cellsCountZ;
            f32 _padding1;
        };

        struct surfel_grid_cell
        {
            u32 surfelsCount;
            u32 surfels[g_maxSurfelsPerCell];
        };

        struct surfel_stack_header
        {
            u32 available;
        };

        struct surfel_tile_data
        {
            f32 worstPixelCoverage;
            surfel_spawn_data spawnData;
        };

        struct surfel_lighting_data
        {
            vec3 irradiance; // Just temporary before implementing SH lighting, irradiance from the sun
            f32 _padding;
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

        stackInitialized = false;
    }

    void surfel_initializer::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::compute);

        const auto gridBounds = ctx.access(inGridBounds);
        const auto gridCellSize = ctx.access(inGridCellSize);
        const auto maxSurfels = ctx.access(inMaxSurfels);

        const auto cellsCount = (gridBounds.max - gridBounds.min) / gridCellSize;

        const u32 surfelsStackSize = sizeof(u32) * (maxSurfels + 1);
        const u32 SurfelsSpawnDataSize = sizeof(surfel_spawn_data) * maxSurfels;
        const u32 surfelsDataSize = sizeof(surfel_dynamic_data) * maxSurfels;
        const u32 surfelsLightingDataSize = sizeof(surfel_lighting_data) * maxSurfels;
        const u32 surfelsGridSize = u32(sizeof(surfel_grid_header) +
            sizeof(surfel_grid_cell) * std::ceil(cellsCount.x) * std::ceil(cellsCount.y) * std::ceil(cellsCount.z));

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

        // Probably only one of the two reallyneeds to be stable at a given time
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

        // The grid doesn't necessarily need to be stable I guess, we rebuild it every frame
        ctx.create(outSurfelsGrid,
            buffer_resource_initializer{
                .size = surfelsGridSize,
                .isStable = true,
            },
            buffer_usage::storage_write);

        stackInitialized = stackInitialized && !ctx.has_event<gi_reset_event>();
    }

    void surfel_initializer::execute(const frame_graph_execute_context& ctx)
    {
        const bool mustReinitialize =
            (ctx.get_frames_alive_count(outSurfelsStack) != ctx.get_frames_alive_count(outSurfelsSpawnData) ||
                ctx.get_frames_alive_count(outSurfelsStack) != ctx.get_frames_alive_count(outSurfelsGrid));

        // Initialize the grid every frame, we fill it after updating/spawning
        auto& pm = ctx.get_pass_manager();

        // Re-initialize when buffers changed (it might not be a perfect check, but probably good enough)
        stackInitialized = stackInitialized || mustReinitialize;

        // We only need to initialize the stack once, but we could also run this code to reset surfels
        if (stackInitialized)
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

            stackInitialized = true;
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

        const auto tileOutputBuffer = outFullTileCoverage;

        if (const auto tiling = pm.begin_compute_pass(commandBuffer, tilingPipeline))
        {

            ctx.bind_buffers(bindingTable,
                {
                    {"b_InstanceTables", inInstanceTables},
                    {"b_MeshTables", inMeshDatabase},
                    {"b_CameraBuffer", inCameraBuffer},
                    {"b_SurfelsGrid", inSurfelsGrid},
                    {"b_SurfelsData", inSurfelsData},
                    {"b_OutTileCoverage", outFullTileCoverage},
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

        ctx.acquire(inOutSurfelsGrid, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsSpawnData, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsData, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsStack, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsLightingData0, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsLightingData1, buffer_usage::storage_write);
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
                {"b_SurfelsGrid", inOutSurfelsGrid},
                {"b_SurfelsSpawnData", inOutSurfelsSpawnData},
                {"b_SurfelsData", inOutSurfelsData},
                {"b_SurfelsStack", inOutSurfelsStack},
                {"b_InSurfelsLighting", inOutSurfelsLightingData0},
                {"b_OutSurfelsLighting", inOutSurfelsLightingData1},
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
                };

                const push_constants constants{
                    .srcElements = tileCoverage.elements,
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
            .shaderSourcePath = "./vulkan/shaders/surfels/initialize_grid.comp",
        });

        OBLO_ASSERT(initGridPass);
    }

    void surfel_grid_clear::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::compute);
        ctx.acquire(inOutSurfelsGrid, buffer_usage::storage_write);

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
                });

            const binding_table* bindingTables[] = {
                &bindings,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            const auto gridBounds = ctx.access(inGridBounds);
            const auto gridCellSize = ctx.access(inGridCellSize);

            const auto cellsCount = (gridBounds.max - gridBounds.min) / gridCellSize;

            const auto centroid = ctx.access(outCameraCentroid);

            const surfel_grid_header header{
                .boundsMin = gridBounds.min + centroid,
                .cellSize = gridCellSize,
                .boundsMax = gridBounds.max + centroid,
                .cellsCountX = u32(std::ceil(cellsCount.x)),
                .cellsCountY = u32(std::ceil(cellsCount.y)),
                .cellsCountZ = u32(std::ceil(cellsCount.z)),
            };

            const auto subgroupSize = pm.get_subgroup_size();

            const auto groupsX = round_up_div(header.cellsCountX, subgroupSize);
            const auto groupsY = header.cellsCountY;
            const auto groupsZ = header.cellsCountZ;

            pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span(&header, 1)));

            vkCmdDispatch(ctx.get_command_buffer(), groupsX, groupsY, groupsZ);

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

        OBLO_ASSERT(updatePass);
    }

    void surfel_update::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::compute);

        ctx.acquire(inOutSurfelsGrid, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsSpawnData, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsData, buffer_usage::storage_write);

        ctx.acquire(inEntitySetBuffer, buffer_usage::storage_read);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);
        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void surfel_update::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_buffers(bindingTable,
            {
                {"b_SurfelsGrid", inOutSurfelsGrid},
                {"b_SurfelsSpawnData", inOutSurfelsSpawnData},
                {"b_SurfelsData", inOutSurfelsData},
                {"b_SurfelsStack", inOutSurfelsStack},
                {"b_InstanceTables", inInstanceTables},
                {"b_MeshTables", inMeshDatabase},
                {"b_EcsEntitySet", inEntitySetBuffer},
            });

        const auto commandBuffer = ctx.get_command_buffer();

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

    void surfel_raytracing::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        rtPass = passManager.register_raytracing_pass({
            .name = "Surfel Ray-Tracing",
            .generation = "./vulkan/shaders/surfels/generate_rays.rgen",
            .miss =
                {
                    "./vulkan/shaders/surfels/skybox.rmiss",
                },
            .hitGroups =
                {
                    {
                        .type = raytracing_hit_type::triangle,
                        .shaders = {"./vulkan/shaders/surfels/first_ray.rchit"},
                    },
                },
        });

        outputSelector = 0;
    }

    void surfel_raytracing::build(const frame_graph_build_context& ctx)
    {
        // Last frame output becomes the input
        const auto inputSelector = outputSelector;
        outputSelector = 1 - outputSelector;

        ctx.begin_pass(pass_kind::raytracing);

        ctx.acquire(inOutSurfelsGrid, buffer_usage::storage_read);
        ctx.acquire(inOutSurfelsData, buffer_usage::storage_read);

        const resource<buffer> lightingDataBuffers[] = {
            inSurfelsLightingData0,
            inSurfelsLightingData1,
        };

        ctx.reroute(lightingDataBuffers[inputSelector], lastFrameSurfelsLightingData);
        ctx.reroute(lightingDataBuffers[outputSelector], outSurfelsLightingData);

        ctx.acquire(lastFrameSurfelsLightingData, buffer_usage::storage_read);
        ctx.acquire(outSurfelsLightingData, buffer_usage::storage_write);

        ctx.acquire(inLightConfig, buffer_usage::uniform);
        ctx.acquire(inLightBuffer, buffer_usage::storage_read);

        ctx.acquire(inSkyboxSettingsBuffer, buffer_usage::uniform);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

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
                {"b_SurfelsData", inOutSurfelsData},
                {"b_InSurfelsLighting", lastFrameSurfelsLightingData},
                {"b_OutSurfelsLighting", outSurfelsLightingData},
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
                u32 randomSeed;
            } constants{
                .randomSeed = randomSeed,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);
            pm.push_constants(*pass, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, as_bytes(std::span{&constants, 1}));

            pm.trace_rays(*pass, maxSurfels, 1, 1);

            pm.end_raytracing_pass(*pass);
        }
    }
}