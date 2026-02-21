#include <oblo/vulkan/nodes/debug/raytracing_debug.hpp>

#include <oblo/core/unreachable.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/draw/raytracing_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/loaded_functions.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo
{
    void raytracing_debug::init(const frame_graph_init_context& ctx)
    {
        rtDebugPass = ctx.register_raytracing_pass({
            .name = "Ray-Tracing Debug Pass",
            .generation = "./vulkan/shaders/raytracing_debug/rtdebug.rgen",
            .miss =
                {
                    "./vulkan/shaders/raytracing_debug/rtdebug.rmiss",
                    "./vulkan/shaders/raytracing_debug/rtdebug_shadow.rmiss",
                },
            .hitGroups =
                {
                    {
                        .type = raytracing_hit_type::triangle,
                        .shaders = {"./vulkan/shaders/raytracing_debug/rtdebug.rchit"},
                    },
                },
        });
    }

    void raytracing_debug::build(const frame_graph_build_context& ctx)
    {
        rtDebugPassInstance = ctx.raytracing_pass(rtDebugPass, {});

        const auto resolution = ctx.access(inResolution);

        ctx.create(outShadedImage,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = texture_format::r16g16b16a16_sfloat,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);

        ctx.acquire(inLightConfig, buffer_usage::uniform);
        ctx.acquire(inLightBuffer, buffer_usage::storage_read);

        ctx.acquire(inSkyboxSettingsBuffer, buffer_usage::uniform);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void raytracing_debug::execute(const frame_graph_execute_context& ctx)
    {
        binding_table bindingTable;

        bindingTable.bind_buffers({
            {"b_InstanceTables"_hsv, inInstanceTables},
            {"b_MeshTables"_hsv, inMeshDatabase},
            {"b_CameraBuffer"_hsv, inCameraBuffer},
            {"b_LightConfig"_hsv, inLightConfig},
            {"b_LightData"_hsv, inLightBuffer},
            {"b_SkyboxSettings"_hsv, inSkyboxSettingsBuffer},
        });

        bindingTable.bind_textures({
            {"t_OutShadedImage"_hsv, outShadedImage},
        });

        bindingTable.bind("u_SceneTLAS"_hsv, ctx.get_global_tlas());

        if (const auto pass = ctx.begin_pass(rtDebugPassInstance))
        {
            const auto resolution = ctx.access(inResolution);

            ctx.bind_descriptor_sets(bindingTable);

            ctx.trace_rays(resolution.x, resolution.y, 1);

            ctx.end_pass();
        }
    }
}