#include <oblo/vulkan/nodes/ao/rtao.hpp>

#include <oblo/core/random_generator.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/raytracing_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    void rtao::init(const frame_graph_init_context& ctx)
    {
        rtPass = ctx.register_raytracing_pass({
            .name = "Ray-Traced Ambient Occlusion",
            .generation = "./vulkan/shaders/ao/rtao.rgen",
            .miss = {"./vulkan/shaders/ao/rtao.rmiss"},
            .hitGroups =
                {
                    {
                        .type = raytracing_hit_type::triangle,
                        .shaders = {"./vulkan/shaders/ao/rtao.rchit"},
                    },
                },
        });
    }

    void rtao::build(const frame_graph_build_context& ctx)
    {
        rtPassInstance = ctx.raytracing_pass(rtPass, {});

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        const auto visBufferInit = ctx.get_current_initializer(inVisibilityBuffer).value_or({});

        ctx.create(outAO,
            {
                .width = visBufferInit.width,
                .height = visBufferInit.height,
                .format = texture_format::r8_unorm,
            },
            texture_usage::storage_write);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);
        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

        randomSeed = ctx.get_random_generator().generate();
    }

    void rtao::execute(const frame_graph_execute_context& ctx)
    {
        binding_table bindingTable;

        bindingTable.bind_buffers({
            {"b_CameraBuffer"_hsv, inCameraBuffer},
            {"b_InstanceTables"_hsv, inInstanceTables},
            {"b_MeshTables"_hsv, inMeshDatabase},
        });

        bindingTable.bind_textures({
            {"t_InVisibilityBuffer"_hsv, inVisibilityBuffer},
            {"t_OutAO"_hsv, outAO},
        });

        bindingTable.bind("u_SceneTLAS"_hsv, ctx.get_global_tlas());

        if (const auto pass = ctx.begin_pass(rtPassInstance))
        {
            const auto resolution = ctx.get_resolution(outAO);

            struct push_constants
            {
                u32 randomSeed;
            };

            const push_constants constants{
                .randomSeed = randomSeed,
            };

            ctx.bind_descriptor_sets(bindingTable);

            ctx.push_constants(shader_stage::raygen, 0, as_bytes(std::span{&constants, 1}));

            ctx.trace_rays(resolution.x, resolution.y, 1);

            ctx.end_pass();
        }
    }
}