#include <oblo/vulkan/nodes/visibility/visibility_temporal.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    void visibility_temporal::init(const frame_graph_init_context& ctx)
    {
        temporalPass = ctx.register_compute_pass({
            .name = "Visibility Temporal Pass",
            .shaderSourcePath = "./vulkan/shaders/visibility/visibility_temporal.comp",
        });
    }

    void visibility_temporal::build(const frame_graph_build_context& ctx)
    {
        temporalPassInstance = ctx.compute_pass(temporalPass, {});

        const auto imageInitializer = ctx.get_current_initializer(inVisibilityBuffer);
        imageInitializer.assert_value();

        ctx.create(outMotionVectors,
            {
                .width = imageInitializer->width,
                .height = imageInitializer->height,
                .format = texture_format::r8g8_unorm,
            },
            texture_usage::storage_write);

        ctx.create(outDisocclusionMask,
            {
                .width = imageInitializer->width,
                .height = imageInitializer->height,
                .format = texture_format::r8_unorm,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        ctx.acquire(inLastFrameDepth, texture_usage::shader_read);
        ctx.acquire(inCurrentDepth, texture_usage::shader_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void visibility_temporal::execute(const frame_graph_execute_context& ctx)
    {
        if (const auto pass = ctx.begin_pass(temporalPassInstance))
        {
            const vec2u resolution = ctx.get_resolution(inVisibilityBuffer);

            binding_table bindingTable;

            bindingTable.bind_buffers({
                {"b_InstanceTables"_hsv, inInstanceTables},
                {"b_MeshTables"_hsv, inMeshDatabase},
                {"b_CameraBuffer"_hsv, inCameraBuffer},
            });

            bindingTable.bind_textures({
                {"t_OutMotionVectors"_hsv, outMotionVectors},
                {"t_OutDisocclusionMask"_hsv, outDisocclusionMask},
                {"t_InVisibilityBuffer"_hsv, inVisibilityBuffer},
                {"t_InCurrentDepth"_hsv, inCurrentDepth},
                {"t_InLastFrameDepth"_hsv, inLastFrameDepth},
            });

            ctx.bind_descriptor_sets(bindingTable);

            ctx.dispatch_compute(round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);

            ctx.end_pass();
        }
    }
}