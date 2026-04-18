#include <oblo/renderer/nodes/pathtracing/pathtracing.hpp>

#include <oblo/core/random_generator.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/data/draw_buffer_data.hpp>
#include <oblo/renderer/draw/binding_table.hpp>
#include <oblo/renderer/draw/raytracing_pass_initializer.hpp>
#include <oblo/renderer/graph/node_common.hpp>

namespace oblo
{
    void pathtracing::init(const frame_graph_init_context& ctx)
    {
        ptPass = ctx.register_raytracing_pass({
            .name = "Path-Tracing Pass",
            .generation = "./vulkan/shaders/pathtracing/pathtracing_gen.rgen",
            .miss =
                {
                    "./vulkan/shaders/pathtracing/pathtracing_miss.rmiss",
                    "./vulkan/shaders/pathtracing/pathtracing_shadow.rmiss",
                },
            .hitGroups =
                {
                    {
                        .type = gpu::raytracing_hit_type::triangle,
                        .shaders = {"./vulkan/shaders/pathtracing/pathtracing_hit.rchit"},
                    },
                },
        });
    }

    void pathtracing::build(const frame_graph_build_context& ctx)
    {
        ptPassInstance = ctx.raytracing_pass(ptPass, {});

        const auto resolution = ctx.access(inResolution);

        ctx.create(outShadedImage,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = gpu::image_format::r16g16b16a16_sfloat,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);

        ctx.acquire(inLightConfig, buffer_usage::uniform);
        ctx.acquire(inLightBuffer, buffer_usage::storage_read);

        ctx.acquire(inSkyboxSettingsBuffer, buffer_usage::uniform);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        ctx.acquire(inLightBuffer, buffer_usage::storage_read);

        ctx.acquire(inDisocclusionMask, texture_usage::storage_read);
        ctx.acquire(inMotionVectors, texture_usage::storage_read);

        ctx.acquire(inGBuffer0, texture_usage::storage_read);
        ctx.acquire(inGBuffer1, texture_usage::storage_read);
        ctx.acquire(inGBuffer2, texture_usage::storage_read);
        ctx.acquire(inGBuffer3, texture_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

        randomSeed = ctx.get_random_generator().generate();
    }

    void pathtracing::execute(const frame_graph_execute_context& ctx)
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
            {"t_InDisocclusionMask"_hsv, inDisocclusionMask},
            {"t_InMotionVectors"_hsv, inMotionVectors},
            {"t_InGBuffer0"_hsv, inGBuffer0},
            {"t_InGBuffer1"_hsv, inGBuffer1},
            {"t_InGBuffer2"_hsv, inGBuffer2},
            {"t_InGBuffer3"_hsv, inGBuffer3},
        });

        bindingTable.bind("u_SceneTLAS"_hsv, ctx.get_global_tlas());

        if (const auto pass = ctx.begin_pass(ptPassInstance))
        {
            const auto resolution = ctx.access(inResolution);

            struct push_constants
            {
                u32 randomSeed;
                f32 aoBias;
            };

            const push_constants constants{
                .randomSeed = randomSeed,
            };

            ctx.bind_descriptor_sets(bindingTable);

            ctx.push_constants(gpu::shader_stage::raygen, 0, as_bytes(std::span{&constants, 1}));

            ctx.trace_rays(resolution.x, resolution.y, 1);

            ctx.end_pass();
        }
    }
}