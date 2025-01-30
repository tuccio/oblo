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
                    {string_builder{}.format("SHADOW_FILTER_PASS_INDEX {}", passIndex).as<hashed_string_view>()},
                }},
            });

        ctx.acquire(inSource, texture_usage::storage_read);
        // ctx.acquire(inMoments, texture_usage::storage_read);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        // ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        // ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        // acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

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

        // if (passIndex == 0)
        //{
        //     ctx.create(stableHistory,
        //         {
        //             .width = imageInitializer->extent.width,
        //             .height = imageInitializer->extent.height,
        //             .format = imageInitializer->format,
        //             .usage = imageInitializer->usage,
        //             .isStable = true,
        //         },
        //         texture_usage::storage_write);

        //    ctx.create(transientHistory,
        //        {
        //            .width = imageInitializer->extent.width,
        //            .height = imageInitializer->extent.height,
        //            .format = imageInitializer->format,
        //            .usage = imageInitializer->usage,
        //        },
        //        texture_usage::storage_write);

        //    ctx.create(historySamples,
        //        {
        //            .width = imageInitializer->extent.width,
        //            .height = imageInitializer->extent.height,
        //            .format = VK_FORMAT_R8_UINT,
        //            .usage = imageInitializer->usage,
        //        },
        //        texture_usage::storage_write);
        //}
        // else
        //{
        //    ctx.acquire(transientHistory, texture_usage::storage_read);
        //    ctx.acquire(historySamples, texture_usage::storage_read);
        //}
    }

    void shadow_filter::execute(const frame_graph_execute_context& ctx)
    {
        if (const auto pass = ctx.begin_pass(filterPassInstance))
        {
            const auto& sourceTexture = ctx.access(inSource);
            const vec2u resolution{sourceTexture.initializer.extent.width, sourceTexture.initializer.extent.height};

            binding_table bindingTable;

            bindingTable.bind_buffers({
                //{"b_InstanceTables", inInstanceTables},
                //{"b_MeshTables", inMeshDatabase},
                {"b_CameraBuffer", inCameraBuffer},
            });

            bindingTable.bind_textures({
                {"t_InSource", inSource},
                //{"t_InMoments", inMoments},
                //{"t_InVisibilityBuffer", inVisibilityBuffer},
                {"t_OutFiltered", outFiltered},
                //{"t_TransientHistory", transientHistory},
                //{"t_HistorySamples", historySamples},
            });

            // if (passIndex == 0)
            //{
            //     // Pass #0 will copy the stable to the transient for the current frame to consume, while also
            //     outputting
            //     // its result as history for next frame
            //     ctx.bind_textures(bindingTable,
            //         {
            //             {"t_StableHistory", stableHistory},
            //         });
            // }

            ctx.bind_descriptor_sets(bindingTable);

            ctx.dispatch_compute(round_up_div(resolution.x, 8u), round_up_div(resolution.x, 8u), 1);

            ctx.end_pass();
        }
    }
}