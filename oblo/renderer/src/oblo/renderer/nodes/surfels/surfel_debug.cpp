#include <oblo/vulkan/nodes/surfels/surfel_debug.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/math/constants.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo
{
    void surfel_debug::init(const frame_graph_init_context& ctx)
    {
        debugPass = ctx.register_compute_pass({
            .name = "GI Debug Pass",
            .shaderSourcePath = "./vulkan/shaders/surfels/surfel_debug_view.comp",
        });
    }

    void surfel_debug::build(const frame_graph_build_context& ctx)
    {
        hashed_string_view define;

        switch (ctx.access(inMode))
        {
        case mode::surfel_grid_id:
            define = "MODE_SURFEL_GRID_ID"_hsv;
            break;

        case mode::surfel_lighting:
            define = "MODE_SURFEL_LIGHTING"_hsv;
            break;

        case mode::surfel_raycount:
            define = "MODE_SURFEL_RAYCOUNT"_hsv;
            break;

        case mode::surfel_inconsistency:
            define = "MODE_SURFEL_INCONSISTENCY"_hsv;
            break;

        case mode::surfel_lifetime:
            define = "MODE_SURFEL_LIFETIME"_hsv;
            break;

        default:
            unreachable();
        }

        debugPassInstance = ctx.compute_pass(debugPass, {.defines = std::span(&define, 1)});

        const auto& visBufferInit = ctx.get_current_initializer(inVisibilityBuffer).value();
        const vec2u resolution = {visBufferInit.width, visBufferInit.height};

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);

        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);
        ctx.acquire(inImage, texture_usage::storage_read);

        ctx.create(outDebugImage,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = texture_format::r8g8b8a8_unorm,
            },
            texture_usage::storage_write);

        ctx.acquire(inSurfelsData, buffer_usage::storage_read);
        ctx.acquire(inSurfelsSpawnData, buffer_usage::storage_read);
        ctx.acquire(inSurfelsGrid, buffer_usage::storage_read);
        ctx.acquire(inSurfelsGridData, buffer_usage::storage_read);
        ctx.acquire(inSurfelsLightingData, buffer_usage::storage_read);
        ctx.acquire(inSurfelsLightEstimatorData, buffer_usage::storage_read);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void surfel_debug::execute(const frame_graph_execute_context& ctx)
    {
        if (const auto pass = ctx.begin_pass(debugPassInstance))
        {
            binding_table bindingTable;

            bindingTable.bind_buffers({
                {"b_CameraBuffer"_hsv, inCameraBuffer},
                {"b_SurfelsData"_hsv, inSurfelsData},
                {"b_SurfelsSpawnData"_hsv, inSurfelsSpawnData},
                {"b_SurfelsGrid"_hsv, inSurfelsGrid},
                {"b_SurfelsGridData"_hsv, inSurfelsGridData},
                {"b_InSurfelsLighting"_hsv, inSurfelsLightingData},
                {"b_InstanceTables"_hsv, inInstanceTables},
                {"b_MeshTables"_hsv, inMeshDatabase},
                {"b_CameraBuffer"_hsv, inCameraBuffer},
                {"b_SurfelsLightEstimator"_hsv, inSurfelsLightEstimatorData},
            });

            bindingTable.bind_textures({
                {"t_InVisibilityBuffer"_hsv, inVisibilityBuffer},
                {"t_InImage"_hsv, inImage},
                {"t_OutShadedImage"_hsv, outDebugImage},
            });

            const vec2u resolution = ctx.get_resolution(inVisibilityBuffer);

            ctx.bind_descriptor_sets(bindingTable);

            ctx.dispatch_compute(round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);

            ctx.end_pass();
        }
    }
}