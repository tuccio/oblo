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
        temporalPass = ctx.register_compute_pass({
            .name = "Shadow Temporal Pass",
            .shaderSourcePath = "./vulkan/shaders/shadows/shadow_temporal.comp",
        });
    }

    void shadow_temporal::build(const frame_graph_build_context& ctx)
    {
        temporalPassInstance = ctx.compute_pass(temporalPass, {});

        ctx.acquire(inShadow, texture_usage::storage_read);
        ctx.acquire(inShadowMean, texture_usage::storage_read);

        const auto imageInitializer = ctx.get_current_initializer(inShadow);
        imageInitializer.assert_value();

        ctx.create(outFiltered,
            {
                .width = imageInitializer->extent.width,
                .height = imageInitializer->extent.height,
                .format = VK_FORMAT_R8_UNORM,
            },
            texture_usage::storage_write);

        ctx.create(outShadowMoments,
            {
                .width = imageInitializer->extent.width,
                .height = imageInitializer->extent.height,
                .format = VK_FORMAT_R16G16_SFLOAT,
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
        if (const auto pass = ctx.begin_pass(temporalPassInstance))
        {
            const auto& sourceTexture = ctx.access(inShadow);
            const vec2u resolution{sourceTexture.initializer.extent.width, sourceTexture.initializer.extent.height};

            binding_table bindingTable;

            bindingTable.bind_buffers({
                {"b_InstanceTables", inInstanceTables},
                {"b_MeshTables", inMeshDatabase},
                {"b_CameraBuffer", inCameraBuffer},
            });

            bindingTable.bind_textures({
                {"t_InShadow", inShadow},
                {"t_InShadowMean", inShadowMean},
                {"t_InHistory", inHistory},
                {"t_OutFiltered", outFiltered},
                {"t_OutShadowMoments", outShadowMoments},
                {"t_InOutHistorySamplesCount", inOutHistorySamplesCount},
                {"t_InVisibilityBuffer", inVisibilityBuffer},
            });

            ctx.bind_descriptor_sets(bindingTable);

            ctx.dispatch_compute(round_up_div(resolution.x, 8u), round_up_div(resolution.x, 8u), 1);

            ctx.end_pass();
        }
    }
}