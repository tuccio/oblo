#include <oblo/vulkan/nodes/shadows/shadow_temporal.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    void shadow_temporal::init(const frame_graph_init_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        temporalPass = pm.register_compute_pass({
            .name = "Shadow Temporal Pass",
            .shaderSourcePath = "./vulkan/shaders/shadows/shadow_temporal.comp",
        });

        ctx.set_pass_kind(pass_kind::compute);
    }

    void shadow_temporal::build(const frame_graph_build_context& ctx)
    {
        ctx.acquire(inShadow, texture_usage::storage_read);
        ctx.acquire(inMoments, texture_usage::storage_read);

        const auto imageInitializer = ctx.get_current_initializer(inShadow);
        imageInitializer.assert_value();

        ctx.create(outFiltered,
            {
                .width = imageInitializer->extent.width,
                .height = imageInitializer->extent.height,
                .format = VK_FORMAT_R8_UNORM,
            },
            texture_usage::storage_write);

        // A little weird to create this readonly texture, it will be written later by the first filter pass
        // We effectively read the history from the previous frame in this pass
        ctx.create(inHistory,
            {
                .width = imageInitializer->extent.width,
                .height = imageInitializer->extent.height,
                .format = VK_FORMAT_R8_UNORM,
                .isStable = true,
            },
            texture_usage::storage_read);

        ctx.create(inOutHistorySamplesCount,
            {
                .width = imageInitializer->extent.width,
                .height = imageInitializer->extent.height,
                .format = VK_FORMAT_R8_UINT,
                .isStable = true,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void shadow_temporal::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        const auto commandBuffer = ctx.get_command_buffer();

        const auto pipeline = pm.get_or_create_pipeline(temporalPass, {});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
        {
            const auto& sourceTexture = ctx.access(inShadow);
            const vec2u resolution{sourceTexture.initializer.extent.width, sourceTexture.initializer.extent.height};

            binding_table bindingTable;

            ctx.bind_buffers(bindingTable,
                {
                    {"b_InstanceTables", inInstanceTables},
                    {"b_MeshTables", inMeshDatabase},
                    {"b_CameraBuffer", inCameraBuffer},
                });

            ctx.bind_textures(bindingTable,
                {
                    {"t_InShadow", inShadow},
                    {"t_InMoments", inMoments},
                    {"t_InHistory", inHistory},
                    {"t_OutFiltered", outFiltered},
                    {"t_InOutHistorySamplesCount", inOutHistorySamplesCount},
                    {"t_InVisibilityBuffer", inVisibilityBuffer},
                });

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            vkCmdDispatch(ctx.get_command_buffer(), round_up_div(resolution.x, 8u), round_up_div(resolution.x, 8u), 1);

            pm.end_compute_pass(*pass);
        }
    }
}