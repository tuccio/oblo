#include <oblo/renderer/nodes/shadows/raytraced_shadows.hpp>

#include <oblo/core/random_generator.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/data/draw_buffer_data.hpp>
#include <oblo/renderer/data/picking_configuration.hpp>
#include <oblo/renderer/draw/binding_table.hpp>
#include <oblo/renderer/draw/raytracing_pass_initializer.hpp>
#include <oblo/renderer/graph/node_common.hpp>
#include <oblo/renderer/loaded_functions.hpp>
#include <oblo/renderer/utility.hpp>

namespace oblo
{
    void raytraced_shadows::init(const frame_graph_init_context& ctx)
    {
        shadowPass = ctx.register_raytracing_pass({
            .name = "Ray-Traced Shadows",
            .generation = "./vulkan/shaders/shadows/rtshadows.rgen",
            .miss = {"./vulkan/shaders/shadows/rtshadows.rmiss"},
            .hitGroups =
                {
                    {
                        .type = raytracing_hit_type::triangle,
                        .shaders = {"./vulkan/shaders/shadows/rtshadows.rchit"},
                    },
                },
        });
    }

    void raytraced_shadows::build(const frame_graph_build_context& ctx)
    {
        const auto& cfg = ctx.access(inConfig);

        string_builder shadowType;
        shadowType.format("SHADOW_TYPE {}", u32(cfg.type));

        string_builder shadowHard;
        shadowHard.format("SHADOW_HARD {}", u32{cfg.hardShadows});

        const hashed_string_view defines[] = {shadowType.as<hashed_string_view>(), shadowHard.as<hashed_string_view>()};

        shadowPassInstance = ctx.raytracing_pass(shadowPass, {.defines = defines});

        const auto resolution = ctx.access(inResolution);

        ctx.create(outShadow,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = texture_format::r8_unorm,
            },
            texture_usage::storage_write);

        ctx.acquire(inDepthBuffer, texture_usage::shader_read);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inLightBuffer, buffer_usage::storage_read);

        randomSeed = ctx.get_random_generator().generate();
    }

    void raytraced_shadows::execute(const frame_graph_execute_context& ctx)
    {
        binding_table bindingTable;

        bindingTable.bind_buffers({
            {"b_CameraBuffer", inCameraBuffer},
            {"b_LightData", inLightBuffer},
        });

        bindingTable.bind_textures({
            {"t_InDepthBuffer", inDepthBuffer},
            {"t_OutShadow", outShadow},
        });

        bindingTable.bind("u_SceneTLAS", ctx.get_global_tlas());

        if (const auto pass = ctx.begin_pass(shadowPassInstance))
        {
            const auto& cfg = ctx.access(inConfig);
            const auto resolution = ctx.access(inResolution);

            struct push_constants
            {
                u32 randomSeed;
                u32 lightIndex;
                f32 punctualLightRadius;
            };

            const push_constants constants{
                .randomSeed = randomSeed,
                .lightIndex = cfg.lightIndex,
                .punctualLightRadius = cfg.shadowPunctualRadius,
            };

            ctx.bind_descriptor_sets(bindingTable);

            ctx.push_constants(shader_stage::raygen, 0, as_bytes(std::span{&constants, 1}));

            ctx.trace_rays(resolution.x, resolution.y, 1);

            ctx.end_pass();
        }
    }
}