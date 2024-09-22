#include <oblo/vulkan/nodes/surfels/surfel_management.hpp>

#include <oblo/math/vec4.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    namespace
    {
        struct surfel_data
        {
            u32 nextInCell;
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
            u32 firstSurfel;
        };

        struct surfel_stack_header
        {
            u32 available;
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

        initGridPass = pm.register_compute_pass({
            .name = "Initialize Grid Pool",
            .shaderSourcePath = "./vulkan/shaders/surfels/initialize_grid.comp",
        });

        OBLO_ASSERT(initGridPass);

        ctx.set_pass_kind(pass_kind::compute);

        structuresInitialized = false;
    }

    void surfel_initializer::build(const frame_graph_build_context& ctx)
    {
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

        ctx.create(outSurfelsGrid,
            buffer_resource_initializer{
                .size = surfelsGridSize,
                .isStable = true,
            },
            buffer_usage::storage_write);
    }

    void surfel_initializer::execute(const frame_graph_execute_context& ctx)
    {
        OBLO_ASSERT(ctx.get_frames_alive_count(outSurfelsStack) == ctx.get_frames_alive_count(outSurfelsPool));
        OBLO_ASSERT(ctx.get_frames_alive_count(outSurfelsStack) == ctx.get_frames_alive_count(outSurfelsGrid));

        if (structuresInitialized)
        {
            return;
        }

        // Initialize the stack
        auto& pm = ctx.get_pass_manager();

        const auto initStackPipeline = pm.get_or_create_pipeline(initStackPass, {});
        const auto initGridPipeline = pm.get_or_create_pipeline(initGridPass, {});

        bool stackInitialized{};
        bool gridInitialized{};

        if (const auto pass = pm.begin_compute_pass(ctx.get_command_buffer(), initStackPipeline))
        {
            binding_table bindings;

            ctx.bind_buffers(bindings,
                {
                    {"b_SurfelsStack", outSurfelsStack},
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

        if (const auto pass = pm.begin_compute_pass(ctx.get_command_buffer(), initGridPipeline))
        {
            binding_table bindings;

            ctx.bind_buffers(bindings,
                {
                    {"b_SurfelsGrid", outSurfelsGrid},
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

            gridInitialized = true;
        }

        structuresInitialized = gridInitialized && stackInitialized;
    }

    void surfel_spawner::init(const frame_graph_init_context& ctx)
    {
        (void) ctx;
    }

    void surfel_spawner::build(const frame_graph_build_context& ctx)
    {
        (void) ctx;
    }

    void surfel_spawner::execute(const frame_graph_execute_context& ctx)
    {
        (void) ctx;
    }
}