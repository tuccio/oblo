#include <oblo/vulkan/nodes/visibility/visibility_extra_buffers.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    namespace
    {
        void make_defines(dynamic_array<hashed_string_view>& out,
            flags<visibility_extra_buffers::enabled_buffers> enabledBuffers)
        {
            using enabled_buffers = visibility_extra_buffers::enabled_buffers;

            if (enabledBuffers.contains(enabled_buffers::motion_vectors))
            {
                out.emplace_back("OUT_MOTION_VECTORS"_hsv);
            }

            if (enabledBuffers.contains(enabled_buffers::disocclusion_mask))
            {
                out.emplace_back("OUT_DISOCCLUSION_MASK"_hsv);
            }
        }
    }

    void visibility_extra_buffers::init(const frame_graph_init_context& ctx)
    {
        extraBuffersPass = ctx.register_compute_pass({
            .name = "Visibility Temporal Pass",
            .shaderSourcePath = "./vulkan/shaders/visibility/visibility_extra_buffers.comp",
        });
    }

    void visibility_extra_buffers::build(const frame_graph_build_context& ctx)
    {
        enabledBuffers = {};
        extraBuffersPassInstance = {};

        enabledBuffers.assign(enabled_buffers::motion_vectors, ctx.is_active_output(outMotionVectors));
        enabledBuffers.assign(enabled_buffers::disocclusion_mask, ctx.is_active_output(outDisocclusionMask));

        if (enabledBuffers.is_empty())
        {
            return;
        }

        buffered_array<hashed_string_view, u32(enabled_buffers::enum_max)> defines;
        make_defines(defines, enabledBuffers);

        extraBuffersPassInstance = ctx.compute_pass(extraBuffersPass, {defines});

        const auto imageInitializer = ctx.get_current_initializer(inVisibilityBuffer);
        imageInitializer.assert_value();

        if (enabledBuffers.contains(enabled_buffers::motion_vectors))
        {
            ctx.create(outMotionVectors,
                {
                    .width = imageInitializer->width,
                    .height = imageInitializer->height,
                    .format = texture_format::r16g16_sfloat,
                },
                texture_usage::storage_write);
        }

        if (enabledBuffers.contains(enabled_buffers::disocclusion_mask))
        {
            ctx.create(outDisocclusionMask,
                {
                    .width = imageInitializer->width,
                    .height = imageInitializer->height,
                    .format = texture_format::r8_unorm,
                },
                texture_usage::storage_write);
        }

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

        if (enabledBuffers.contains_all(enabled_buffers::disocclusion_mask | enabled_buffers::motion_vectors))
        {
            ctx.acquire(inLastFrameDepth, texture_usage::shader_read);
            ctx.acquire(inCurrentDepth, texture_usage::shader_read);
        }
    }

    void visibility_extra_buffers::execute(const frame_graph_execute_context& ctx)
    {
        OBLO_ASSERT(!enabledBuffers.is_empty());

        if (enabledBuffers.is_empty()) [[unlikely]]
        {
            return;
        }

        if (const auto pass = ctx.begin_pass(extraBuffersPassInstance))
        {
            const vec2u resolution = ctx.get_resolution(inVisibilityBuffer);

            binding_table bindingTable;

            bindingTable.bind_buffers({
                {"b_InstanceTables"_hsv, inInstanceTables},
                {"b_MeshTables"_hsv, inMeshDatabase},
                {"b_CameraBuffer"_hsv, inCameraBuffer},
            });

            bindingTable.bind_textures({
                {"t_InVisibilityBuffer"_hsv, inVisibilityBuffer},
            });

            if (enabledBuffers.contains(enabled_buffers::disocclusion_mask))
            {
                bindingTable.bind_textures({
                    {"t_OutDisocclusionMask"_hsv, outDisocclusionMask},
                });
            }

            if (enabledBuffers.contains(enabled_buffers::motion_vectors))
            {
                bindingTable.bind_textures({
                    {"t_OutMotionVectors"_hsv, outMotionVectors},
                });
            }

            if (enabledBuffers.contains_any(enabled_buffers::disocclusion_mask | enabled_buffers::motion_vectors))
            {
                bindingTable.bind_textures({
                    {"t_InCurrentDepth"_hsv, inCurrentDepth},
                    {"t_InLastFrameDepth"_hsv, inLastFrameDepth},
                });
            }

            ctx.bind_descriptor_sets(bindingTable);

            ctx.dispatch_compute(round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);

            ctx.end_pass();
        }
    }
}