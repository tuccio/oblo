#include <oblo/vulkan/nodes/shadows/raytraced_shadows.hpp>

#include <oblo/core/random_generator.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/draw/raytracing_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/loaded_functions.hpp>
#include <oblo/vulkan/nodes/frustum_culling.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    void raytraced_shadows::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        shadowPass = passManager.register_raytracing_pass({
            .name = "Ray-Traced Shadows",
            .generation = "./vulkan/shaders/shadows/rtshadows.rgen",
            .miss = "./vulkan/shaders/shadows/rtshadows.rmiss",
            .hitGroups =
                {
                    {
                        .type = raytracing_hit_type::triangle,
                        .shaders = {"./vulkan/shaders/shadows/rtshadows.rchit"},
                    },
                },
        });

        ctx.set_pass_kind(pass_kind::raytracing);
    }

    void raytraced_shadows::build(const frame_graph_build_context& ctx)
    {
        const auto config = ctx.access(inConfig);
        const auto resolution = ctx.access(inResolution);

        ctx.create(outShadow,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = VK_FORMAT_R8_UNORM,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT,
            },
            texture_usage::storage_write);

        ctx.acquire(inDepthBuffer, texture_usage::shader_read);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inLightBuffer, buffer_usage::storage_read);

        randomSeed = ctx.get_random_generator().generate();
    }

    void raytraced_shadows::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_buffers(bindingTable,
            {
                {"b_CameraBuffer", inCameraBuffer},
                {"b_LightData", inLightBuffer},
            });

        ctx.bind_textures(bindingTable,
            {
                {"t_InDepthBuffer", inDepthBuffer},
                {"t_OutShadow", outShadow},
            });

        bindingTable.emplace(ctx.get_string_interner().get_or_add("u_SceneTLAS"),
            make_bindable_object(ctx.get_draw_registry().get_tlas()));

        const auto commandBuffer = ctx.get_command_buffer();
        const auto& cfg = ctx.access(inConfig);

        string_builder shadowType;
        shadowType.format("SHADOW_TYPE {}", u32(cfg.type));

        string_builder shadowHard;
        shadowHard.format("SHADOW_HARD {}", u32{cfg.hardShadows});

        const hashed_string_view defines[] = {shadowType.as<hashed_string_view>(), shadowHard.as<hashed_string_view>()};

        const auto pipeline = pm.get_or_create_pipeline(shadowPass, {.defines = defines});

        if (const auto pass = pm.begin_raytracing_pass(commandBuffer, pipeline))
        {
            const auto resolution = ctx.access(inResolution);

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

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

            pm.bind_descriptor_sets(*pass, bindingTables);

            pm.push_constants(*pass, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, as_bytes(std::span{&constants, 1}));

            pm.trace_rays(*pass, resolution.x, resolution.y, 1);

            pm.end_raytracing_pass(*pass);
        }
    }
}