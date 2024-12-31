#include <oblo/vulkan/nodes/surfels/surfel_management.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/events/gi_reset_event.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    namespace
    {
        constexpr u32 g_surfelTileSize{16};
        constexpr u32 g_surfelMaxPerCell{31};

        struct surfel_data
        {
            vec3 position;
            f32 _padding0;
            vec3 normal;
            f32 _padding1;
        };

        struct surfel_grid_header
        {
            vec3 boundsMin;
            u32 cellsCountX;
            vec3 boundsMax;
            u32 cellsCountY;
            vec3 cellSize;
            u32 cellsCountZ;
        };

        struct surfel_grid_cell
        {
            u32 surfelsCount;
            u32 surfels[g_surfelMaxPerCell];
        };

        struct surfel_stack_header
        {
            u32 available;
        };

        struct surfel_tile_data
        {
            vec3 position;
            f32 _padding;
            vec3 normal;
            f32 coverage;
        };
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

        // TODO: When any of these changes we would need to somehow re-initialize all buffers
        const auto gridBounds = ctx.access(inGridBounds);
        const auto gridCellSize = ctx.access(inGridCellSize);
        const auto maxSurfels = ctx.access(inMaxSurfels);

        const auto cellsCount = (gridBounds.max - gridBounds.min) / gridCellSize;

        const u32 surfelsStackSize = sizeof(u32) * (maxSurfels + 1);
        const u32 surfelsPoolSize = sizeof(surfel_data) * maxSurfels;
        const u32 surfelsGridSize = u32(sizeof(surfel_grid_header) +
            sizeof(surfel_grid_cell) * std::ceil(cellsCount.x) * std::ceil(cellsCount.y) * std::ceil(cellsCount.z));

        // TODO: After creation and initialization happened, the usage could be none to avoid any useless memory barrier
        ctx.create(outSurfelsStack,
            buffer_resource_initializer{
                .size = surfelsStackSize,
                .isStable = true,
            },
            buffer_usage::storage_write);

        ctx.create(outSurfelsPool,
            buffer_resource_initializer{
                .size = surfelsPoolSize,
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
        OBLO_ASSERT(ctx.get_frames_alive_count(outSurfelsStack) == ctx.get_frames_alive_count(outSurfelsPool));
        OBLO_ASSERT(ctx.get_frames_alive_count(outSurfelsStack) == ctx.get_frames_alive_count(outSurfelsGrid));

        // Initialize the grid every frame, we fill it after updating/spawning
        auto& pm = ctx.get_pass_manager();

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
                    {"b_SurfelsPool", outSurfelsPool},
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

    namespace
    {
        struct reduction_config
        {
            u32 inElements;
            u32 outElements;
            bool isFirstPass;
        };

        struct surfel_min_coverage_pass
        {
            resource<buffer> inTileCoverage;
            resource<buffer> inOutMinCoverageBuffer;

            // Only used in the last pass
            resource<buffer> outTileCoverage;

            data<reduction_config> inConfig;

            data<h32<compute_pass>> reductionPass;

            void build(const frame_graph_build_context& ctx)
            {
                ctx.begin_pass(pass_kind::compute);

                const auto& config = ctx.access(inConfig);

                if (config.isFirstPass)
                {
                    // In the first pass we allocate the buffer
                    ctx.create(inOutMinCoverageBuffer,
                        buffer_resource_initializer{
                            .size = narrow_cast<u32>(sizeof(u32) * config.outElements),
                        },
                        buffer_usage::storage_write);
                }
                else
                {
                    ctx.acquire(inOutMinCoverageBuffer, buffer_usage::storage_write);
                }

                // In the last pass we also copy to the output
                if (config.outElements == 1)
                {
                    ctx.create(outTileCoverage,
                        buffer_resource_initializer{
                            .size = sizeof(surfel_tile_data),
                        },
                        buffer_usage::storage_write);
                }
            }
        };
    }

    void surfel_min_coverage::init(const frame_graph_init_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        reductionPass = pm.register_compute_pass({
            .name = "Surfel Tiling Reduction",
            .shaderSourcePath = "./vulkan/shaders/surfels/tile_reduction.comp",
        });

        OBLO_ASSERT(reductionPass);
    }

#if 0
    void surfel_min_coverage::pre_build(const frame_graph_build_context& ctx)
    {
        // One option: add graph template from prebuild
        // Easier to implement, but cannot access data you can access in build (e.g. inputs?)
        //
        // TODO: Calculate

        // TODO
        const u32 subgroupSize = 32;
        const vec2u resolution = ctx.access(inResolution);
        const u32 pixelsCount = resolution.x * resolution.y;

        // With subgroup size N, we run groups of size NxN, each group doing a reduction on the subgroup and then
        // writing the result on shader manager, then finally one group doing the final reduction
        const u32 passCount = log2_round_down_power_of_two(round_up_power_of_two(pixelsCount)) /
            log2_round_down_power_of_two(subgroupSize);

        if (passCount == 0)
        {
            OBLO_ASSERT(false);
            return;
        }

        frame_graph_template reduction;

        dynamic_array<frame_graph_template::vertex_handle> passes{&ctx.get_frame_allocator()};
        passes.reserve(passCount);

        for (usize i = 0; i < passCount; ++i)
        {
            const auto node = reduction.add_node<surfel_min_coverage_pass>();
            passes.emplace_back(node);

            const reduction_config cfg{
                .isFirstPass = i == 0,
            };

            reduction.bind(node, &surfel_min_coverage_pass::reductionPass, reductionPass);
            reduction.bind(node, &surfel_min_coverage_pass::inConfig, cfg);
        }

        for (usize i = 1; i < passes.size(); ++i)
        {
            reduction.connect(passes[i - 1],
                &surfel_min_coverage_pass::inOutMinCoverageBuffer,
                passes[i],
                &surfel_min_coverage_pass::inOutMinCoverageBuffer);

            reduction.connect(passes[i - 1],
                &surfel_min_coverage_pass::inTileCoverage,
                passes[i],
                &surfel_min_coverage_pass::inTileCoverage);
        }

        reduction.make_input(passes.front(), &surfel_min_coverage_pass::inTileCoverage, "InSource");
        reduction.make_output(passes.back(), &surfel_min_coverage_pass::outTileCoverage, "OutTarget");

        const auto subgraph = ctx.add_dynamic_graph(reduction);

        // TODO: Actually inTileCoverage when moving it out
        ctx.connect(&surfel_min_coverage::inTileCoverage, subgraph, "InSource");
        ctx.connect(subgraph, "OutTarget", &surfel_min_coverage::outMinTileCoverage);

        /* reduction.connect(currentNode, &surfel_tiling::outTileCoverage, passes.front(), &surfel_tile_min::inSource);

         reduction.connect(passes.back(),
             &surfel_tile_min::outTarget,
             ctx.get_current_node(),
             &surfel_tiling::outTileCoverage);*/
    }

    void surfel_min_coverage::build(const frame_graph_build_context& ctx)
    {
        // I think the problem with this is that we cannot allocate/propagate pins
        // TODO
        // constexpr u32 passCount = 10;

        // dynamic_array<frame_graph_template::vertex_handle> passes{&ctx.get_frame_allocator()};
        // passes.reserve(passCount);

        // for (usize i = 0; i < passCount; ++i)
        //{
        //     const reduction_config cfg{
        //         .isFirstPass = i == 0,
        //     };

        //    const auto newPass = ctx.add_dynamic_node(surfel_min_coverage_pass{.config = cfg});
        //    passes.emplace_back(passes);
        //}

        // for (usize i = 1; i < passes.size(); ++i)
        //{
        //     ctx.connect(passes[i - 1],
        //         &surfel_min_coverage_pass::inOutMinCoverageBuffer,
        //         passes[i],
        //         &surfel_min_coverage_pass::inOutMinCoverageBuffer);

        //    ctx.connect(passes[i - 1],
        //        &surfel_min_coverage_pass::inTileCoverage,
        //        passes[i],
        //        &surfel_min_coverage_pass::inTileCoverage);
        //}

        // const auto subgraph = ctx.add_subgraph(reduction);

        //// TODO: Actually inTileCoverage when moving it out
        // ctx.connect(&surfel_min_coverage::inTileCoverage, passes.front(), );
        // ctx.connect(subgraph, "OutTarget", &surfel_min_coverage::outMinTileCoverage);
        const auto node = ctx.create_dynamic_node<surfel_min_coverage_pass>();

        ctx.connect(ctx.get_current_node(),
            &surfel_min_coverage::inTileCoverage,
            node,
            surfel_min_coverage::inTileCoverage);
        
        // TODO: Others
    }
#endif // #if 0

    void surfel_tiling::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::compute);

        const auto resolution =
            ctx.get_current_initializer(inVisibilityBuffer).assert_value_or(image_initializer{}).extent;

        const u32 tilesX = round_up_div(resolution.width, g_surfelTileSize);
        const u32 tilesY = round_up_div(resolution.height, g_surfelTileSize);

        outTileCoverage = ctx.create_dynamic_buffer(
            buffer_resource_initializer{
                .size = u32(tilesX * tilesY * sizeof(surfel_tile_data)),
            },
            buffer_usage::storage_write);

        ctx.push(outTileCoverageSink,
            {
                .buffer = outTileCoverage,
                .tilesCount = {.x = tilesX, .y = tilesY},
            });

        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

        if (ctx.has_source(inSurfelsGrid))
        {
            ctx.acquire(inSurfelsGrid, buffer_usage::storage_read);
            ctx.acquire(inSurfelsPool, buffer_usage::storage_read);
        }
    }

    void surfel_tiling::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_buffers(bindingTable,
            {
                {"b_InstanceTables", inInstanceTables},
                {"b_MeshTables", inMeshDatabase},
                {"b_CameraBuffer", inCameraBuffer},
                {"b_SurfelsGrid", inSurfelsGrid},
                {"b_SurfelsPool", inSurfelsPool},
                {"b_OutTileCoverage", outTileCoverage},
            });

        ctx.bind_textures(bindingTable,
            {
                {"t_InVisibilityBuffer", inVisibilityBuffer},
            });

        const auto commandBuffer = ctx.get_command_buffer();

        const auto pipeline = pm.get_or_create_pipeline(tilingPass, {});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
        {
            const auto resolution = ctx.access(inVisibilityBuffer).initializer.extent;

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            const u32 tilesX = round_up_div(resolution.width, g_surfelTileSize);
            const u32 tilesY = round_up_div(resolution.height, g_surfelTileSize);

            pm.bind_descriptor_sets(*pass, bindingTables);

            vkCmdDispatch(ctx.get_command_buffer(), tilesX, tilesY, 1);

            pm.end_compute_pass(*pass);
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
        ctx.acquire(inOutSurfelsPool, buffer_usage::storage_write);
        ctx.acquire(inOutSurfelsStack, buffer_usage::storage_write);
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
                {"b_SurfelsPool", inOutSurfelsPool},
                {"b_SurfelsStack", inOutSurfelsStack},
            });

        const auto commandBuffer = ctx.get_command_buffer();

        const auto pipeline = pm.get_or_create_pipeline(spawnPass, {});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
        {
            const auto subgroupSize = pm.get_subgroup_size();

            for (const auto& tileCoverage : tileCoverageSpan)
            {
                const auto tilesCount = tileCoverage.tilesCount;

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
                pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&tilesCount, 1}));

                const u32 groupsX = round_up_div(tilesCount.x, subgroupSize);
                const u32 groupsY = tilesCount.y;

                vkCmdDispatch(ctx.get_command_buffer(), groupsX, groupsY, 1);
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

            const surfel_grid_header header{
                .boundsMin = gridBounds.min,
                .cellsCountX = u32(std::ceil(cellsCount.x)),
                .boundsMax = gridBounds.max,
                .cellsCountY = u32(std::ceil(cellsCount.y)),
                .cellSize = gridCellSize,
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
        ctx.acquire(inOutSurfelsPool, buffer_usage::storage_write);
    }

    void surfel_update::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_buffers(bindingTable,
            {
                {"b_SurfelsGrid", inOutSurfelsGrid},
                {"b_SurfelsPool", inOutSurfelsPool},
            });

        const auto commandBuffer = ctx.get_command_buffer();

        const auto pipeline = pm.get_or_create_pipeline(updatePass, {});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
        {
            const auto subgroupSize = pm.get_subgroup_size();

            const u32 maxSurfels = ctx.access(inMaxSurfels);

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);
            pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&maxSurfels, 1}));

            const u32 groupsX = round_up_div(maxSurfels, subgroupSize);
            vkCmdDispatch(ctx.get_command_buffer(), groupsX, 1, 1);

            pm.end_compute_pass(*pass);
        }
    }
}