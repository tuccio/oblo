#include <oblo/vulkan/nodes/surfels/surfel_debug.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/math/constants.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    void surfel_debug::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        debugPass = passManager.register_compute_pass({
            .name = "GI Debug Pass",
            .shaderSourcePath = "./vulkan/shaders/surfels/surfel_debug_view.comp",
        });
    }

    void surfel_debug::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::graphics);

        const auto& visBufferInit = ctx.get_current_initializer(inVisibilityBuffer).value();
        const vec2u resolution = {visBufferInit.extent.width, visBufferInit.extent.height};

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);

        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);
        ctx.acquire(inImage, texture_usage::storage_read);

        ctx.create(outDebugImage,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT,
            },
            texture_usage::storage_write);

        ctx.acquire(inSurfelsData, buffer_usage::storage_read);
        ctx.acquire(inSurfelsGrid, buffer_usage::storage_read);
        ctx.acquire(inSurfelsGridData, buffer_usage::storage_read);
        ctx.acquire(inSurfelsLightingData, buffer_usage::storage_read);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void surfel_debug::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        const auto commandBuffer = ctx.get_command_buffer();

        hashed_string_view define;

        switch (ctx.access(inMode))
        {
        case mode::surfel_grid_id:
            define = "MODE_SURFEL_GRID_ID"_hsv;
            break;

        case mode::surfel_lighting:
            define = "MODE_SURFEL_LIGHTING"_hsv;
            break;

        default:
            unreachable();
        }

        const auto pipeline = pm.get_or_create_pipeline(debugPass, {.defines = std::span(&define, 1)});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
        {
            binding_table bindingTable;

            ctx.bind_buffers(bindingTable,
                {
                    {"b_CameraBuffer", inCameraBuffer},
                    {"b_SurfelsData", inSurfelsData},
                    {"b_SurfelsGrid", inSurfelsGrid},
                    {"b_SurfelsGridData", inSurfelsGridData},
                    {"b_InSurfelsLighting", inSurfelsLightingData},
                    {"b_InstanceTables", inInstanceTables},
                    {"b_MeshTables", inMeshDatabase},
                    {"b_CameraBuffer", inCameraBuffer},
                });

            ctx.bind_textures(bindingTable,
                {
                    {"t_InVisibilityBuffer", inVisibilityBuffer},
                    {"t_InImage", inImage},
                    {"t_OutShadedImage", outDebugImage},
                });

            const auto& visBuffer = ctx.access(inVisibilityBuffer);
            const vec2u resolution = {visBuffer.initializer.extent.width, visBuffer.initializer.extent.height};

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            vkCmdDispatch(ctx.get_command_buffer(), round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);

            pm.end_compute_pass(*pass);
        }
    }
}