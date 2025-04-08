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
                .width = imageInitializer->width,
                .height = imageInitializer->height,
                .format = texture_format::r8_unorm,
            },
            texture_usage::storage_write);

        // A little weird to create this readonly texture, it will be written later by the first filter pass
        // We effectively read the history from the previous frame in this pass
        ctx.create(inHistory,
            {
                .width = imageInitializer->width,
                .height = imageInitializer->height,
                .format = texture_format::r8_unorm,
                .isStable = true,
            },
            texture_usage::shader_read);

        ctx.acquire(inMotionVectors, texture_usage::storage_read);
        ctx.acquire(inDisocclusionMask, texture_usage::storage_read);
    }

    void shadow_temporal::execute(const frame_graph_execute_context& ctx)
    {
        if (const auto pass = ctx.begin_pass(temporalPassInstance))
        {
            const vec2u resolution = ctx.get_resolution(inShadow);

            binding_table bindingTable;

            bindingTable.bind_textures({
                {"t_InShadow"_hsv, inShadow},
                {"t_InShadowMean"_hsv, inShadowMean},
                {"t_InHistory"_hsv, inHistory},
                {"t_OutFiltered"_hsv, outFiltered},
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