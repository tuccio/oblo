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

    struct surfel_tiling::subpass_info
    {
        h32<frame_graph_pass> id;
        resource<buffer> outBuffer;
    };

    void surfel_tiling::init(const frame_graph_init_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        tilingPass = pm.register_compute_pass({
            .name = "Surfel Tiling",
            .shaderSourcePath = "./vulkan/shaders/surfels/tiling.comp",
        });

        OBLO_ASSERT(tilingPass);

        reductionPass = pm.register_compute_pass({
            .name = "Surfel Tiling Reduction",
            .shaderSourcePath = "./vulkan/shaders/surfels/tile_reduction.comp",
        });

        OBLO_ASSERT(reductionPass);

        const u32 subgroupSize = pm.get_subgroup_size();
        reductionGroupSize = subgroupSize * subgroupSize;
    }

    void surfel_tiling::build(const frame_graph_build_context& ctx)
    {
        resource<buffer> fullTileCoverage;

        const auto resolution =
            ctx.get_current_initializer(inVisibilityBuffer).assert_value_or(image_initializer{}).extent;

        const u32 tilesX = round_up_div(resolution.width, g_surfelTileSize);
        const u32 tilesY = round_up_div(resolution.height, g_surfelTileSize);
        const u32 tilesCount = tilesX * tilesY;

        const u32 reductionPassesCount =
            u32(std::ceilf(f32(log2_round_down_power_of_two(round_up_power_of_two(tilesCount))) /
                log2_round_down_power_of_two(reductionGroupSize)));

        subpasses = allocate_n_span<subpass_info>(ctx.get_frame_allocator(), 1 + reductionPassesCount);

        const u32 tilesBufferSize = u32(tilesCount * sizeof(surfel_tile_data));

        // First subpass: split the screen in tiles, find the best candidate for each tile
        {
            const auto subpass = ctx.begin_pass(pass_kind::compute);

            fullTileCoverage = ctx.create_dynamic_buffer(
                buffer_resource_initializer{
                    .size = tilesBufferSize,
                },
                buffer_usage::storage_write);

            ctx.push(outTileCoverageSink,
                {
                    .buffer = fullTileCoverage,
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

            subpasses.front() = {
                .id = subpass,
                .outBuffer = fullTileCoverage,
            };
        }

        // Parallel reduction until we only have 1 candidate

        u32 currentBufferSize = round_up_multiple(tilesCount, reductionGroupSize) * sizeof(surfel_tile_data);

        for (u32 i = 1; i <= reductionPassesCount; ++i)
        {
            const auto subpass = ctx.begin_pass(pass_kind::compute);

            ctx.acquire(subpasses[i - 1].outBuffer, buffer_usage::storage_read);

            currentBufferSize = max(currentBufferSize / reductionGroupSize, u32(sizeof(surfel_tile_data)));
            OBLO_ASSERT(currentBufferSize > sizeof(surfel_tile_data) || i == reductionPassesCount);

            const auto newBuffer = ctx.create_dynamic_buffer(
                buffer_resource_initializer{
                    .size = currentBufferSize,
                },
                buffer_usage::storage_write);

            subpasses[i] = {.id = subpass, .outBuffer = newBuffer};
        }

        OBLO_ASSERT(currentBufferSize == sizeof(surfel_tile_data));
    }

    void surfel_tiling::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        const binding_table* bindingTables[] = {
            &bindingTable,
        };

        const auto commandBuffer = ctx.get_command_buffer();

        const auto tilingPipeline = pm.get_or_create_pipeline(tilingPass, {});

        const auto resolution = ctx.access(inVisibilityBuffer).initializer.extent;

        const u32 tilesX = round_up_div(resolution.width, g_surfelTileSize);
        const u32 tilesY = round_up_div(resolution.height, g_surfelTileSize);

        resource<buffer> previousBuffer{};

        if (const auto tiling = pm.begin_compute_pass(commandBuffer, tilingPipeline))
        {

            const auto outBuffer = subpasses.front().outBuffer;

            ctx.bind_buffers(bindingTable,
                {
                    {"b_InstanceTables", inInstanceTables},
                    {"b_MeshTables", inMeshDatabase},
                    {"b_CameraBuffer", inCameraBuffer},
                    {"b_SurfelsGrid", inSurfelsGrid},
                    {"b_SurfelsPool", inSurfelsPool},
                    {"b_OutTileCoverage", outBuffer},
                });

            ctx.bind_textures(bindingTable,
                {
                    {"t_InVisibilityBuffer", inVisibilityBuffer},
                });

            pm.bind_descriptor_sets(*tiling, bindingTables);

            vkCmdDispatch(ctx.get_command_buffer(), tilesX, tilesY, 1);

            pm.end_compute_pass(*tiling);

            previousBuffer = outBuffer;
        }

        const auto reductionPipeline = pm.get_or_create_pipeline(reductionPass, {});

        for (const auto& subpass : subpasses.subspan(1))
        {
            ctx.begin_pass(subpass.id);

            // TODO: Start pass and dispatch
            if (const auto reduction = pm.begin_compute_pass(commandBuffer, reductionPipeline))
            {
                bindingTable.clear();

                ctx.bind_buffers(bindingTable,
                    {
                        {"b_InBuffer", previousBuffer},
                        {"b_OutBuffer", subpass.outBuffer},
                    });

                pm.bind_descriptor_sets(*reduction, bindingTables);

                const u32 inElements = ctx.access(previousBuffer).size / u32{sizeof(surfel_tile_data)};
                const u32 groups = round_up_div(inElements, reductionGroupSize);

                pm.push_constants(*reduction, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&inElements, 1}));

                vkCmdDispatch(ctx.get_command_buffer(), groups, 1, 1);

                pm.end_compute_pass(*reduction);
            }

            previousBuffer = subpass.outBuffer;
        }
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

                vkCmdDispatch(ctx.get_command_buffer(), 1, 1, 1);
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