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

namespace oblo::vk
{
    void raytracing_debug::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        rtDebugPass = passManager.register_raytracing_pass({
            .name = "Ray-Tracing Debug Pass",
            .generation = "./vulkan/shaders/raytracing_debug/rtdebug.rgen",
            .miss = "./vulkan/shaders/raytracing_debug/rtdebug.rmiss",
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
        ctx.begin_pass(pass_kind::raytracing);

        const auto resolution = ctx.access(inResolution);

        ctx.create(outShadedImage,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void raytracing_debug::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_buffers(bindingTable,
            {
                {"b_InstanceTables", inInstanceTables},
                {"b_MeshTables", inMeshDatabase},
                {"b_CameraBuffer", inCameraBuffer},
            });

        ctx.bind_textures(bindingTable,
            {
                {"t_OutShadedImage", outShadedImage},
            });

        bindingTable.emplace(ctx.get_string_interner().get_or_add("u_SceneTLAS"),
            make_bindable_object(ctx.get_draw_registry().get_tlas()));

        const auto commandBuffer = ctx.get_command_buffer();

        const auto pipeline = pm.get_or_create_pipeline(rtDebugPass, {});

        if (const auto pass = pm.begin_raytracing_pass(commandBuffer, pipeline))
        {
            const auto resolution = ctx.access(inResolution);

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            pm.trace_rays(*pass, resolution.x, resolution.y, 1);

            pm.end_raytracing_pass(*pass);
        }
    }
}