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
        ctx.acquire(inMotionVectors, texture_usage::storage_read);
        ctx.acquire(inDisocclusionMask, texture_usage::storage_read);

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
                {"b_InstanceTables"_hsv, inInstanceTables},
                {"b_MeshTables"_hsv, inMeshDatabase},
                {"b_CameraBuffer"_hsv, inCameraBuffer},
            });

            bindingTable.bind_textures({
                {"t_InShadow"_hsv, inShadow},
                {"t_InShadowMean"_hsv, inShadowMean},
                {"t_InHistory"_hsv, inHistory},
                {"t_OutFiltered"_hsv, outFiltered},
                {"t_OutShadowMoments"_hsv, outShadowMoments},
                {"t_InOutHistorySamplesCount"_hsv, inOutHistorySamplesCount},
                {"t_InVisibilityBuffer"_hsv, inVisibilityBuffer},
                {"t_InDisocclusionMask"_hsv, inDisocclusionMask},
                {"t_InMotionVectors"_hsv, inMotionVectors},
            });

            ctx.bind_descriptor_sets(bindingTable);

            struct push_constants
            {
                f32 temporalAccumulationFactor;
            };

            const push_constants constants{
                .temporalAccumulationFactor = ctx.access(inConfig).temporalAccumulationFactor,
            };

            ctx.push_constants(shader_stage::compute, 0, as_bytes(std::span{&constants, 1}));

            ctx.dispatch_compute(round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);

            ctx.end_pass();
        }
    }
}