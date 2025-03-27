#include <oblo/vulkan/nodes/shadows/shadow_filter.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    void shadow_filter::init(const frame_graph_init_context& ctx)
    {
        filterPass = ctx.register_compute_pass({
            .name = string_builder{}.format("Shadow filter #{}", passIndex).view(),
            .shaderSourcePath = "./vulkan/shaders/shadows/shadow_filter.comp",
        });
    }

    void shadow_filter::build(const frame_graph_build_context& ctx)
    {
        filterPassInstance = ctx.compute_pass(filterPass,
            {
                .defines = {{
                    {string_builder{}.format("A_TROUS_PASS_INDEX {}", passIndex).as<hashed_string_view>()},
                }},
            });

        ctx.acquire(inSource, texture_usage::storage_read);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

        const auto imageInitializer = ctx.get_current_initializer(inSource);
        imageInitializer.assert_value();

        if (passIndex == 0)
        {
            // The output of the first pass is used as history for the next frame, so the texture is created earlier by
            // the temporal pass
            ctx.acquire(outFiltered, texture_usage::storage_write);
        }
        else
        {
            ctx.create(outFiltered,
                {
                    .width = imageInitializer->extent.width,
                    .height = imageInitializer->extent.height,
                    .format = imageInitializer->format,
                    .usage = imageInitializer->usage,
                },
                texture_usage::storage_write);
        }
    }

    void shadow_filter::execute(const frame_graph_execute_context& ctx)
    {
        if (const auto pass = ctx.begin_pass(filterPassInstance))
        {
            const auto& sourceTexture = ctx.access(inSource);
            const vec2u resolution{sourceTexture.initializer.extent.width, sourceTexture.initializer.extent.height};

            binding_table bindingTable;

            bindingTable.bind_buffers({
                {"b_InstanceTables"_hsv, inInstanceTables},
                {"b_MeshTables"_hsv, inMeshDatabase},
                {"b_CameraBuffer"_hsv, inCameraBuffer},
            });

            bindingTable.bind_textures({
                {"t_InSource"_hsv, inSource},
                {"t_InVisibilityBuffer"_hsv, inVisibilityBuffer},
                {"t_OutFiltered"_hsv, outFiltered},
            });

            ctx.bind_descriptor_sets(bindingTable);

            struct push_constants
            {
                f32 depthSigma;
            };

            const push_constants constants{
                .depthSigma = ctx.access(inConfig).depthSigma,
            };

            ctx.push_constants(shader_stage::compute, 0, as_bytes(std::span{&constants, 1}));

            ctx.dispatch_compute(round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);

            ctx.end_pass();
        }
    }
}