#include <oblo/vulkan/nodes/shadow_filter.hpp>

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
        auto& pm = ctx.get_pass_manager();

        filterPass = pm.register_compute_pass({
            .name = string_builder{}.format("Shadow filter #{}", passIndex).view(),
            .shaderSourcePath = "./vulkan/shaders/shadows/shadow_filter.comp",
        });

        ctx.set_pass_kind(pass_kind::compute);
    }

    void shadow_filter::build(const frame_graph_build_context& ctx)
    {
        ctx.acquire(inSource, texture_usage::storage_read);

        const auto imageInitializer = ctx.get_current_initializer(inSource);
        imageInitializer.assert_value();

        ctx.create(outFiltered,
            {
                .width = imageInitializer->extent.width,
                .height = imageInitializer->extent.height,
                .format = imageInitializer->format,
                .usage = imageInitializer->usage,
            },
            texture_usage::storage_write);
    }

    void shadow_filter::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        const auto commandBuffer = ctx.get_command_buffer();

        const auto pipeline = pm.get_or_create_pipeline(filterPass,
            {
                .defines = {{
                    {string_builder{}.format("SHADOW_FILTER_PASS_INDEX {}", passIndex).as<hashed_string_view>()},
                }},
            });

        if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
        {
            const auto& sourceTexture = ctx.access(inSource);
            const vec2u resolution{sourceTexture.initializer.extent.width, sourceTexture.initializer.extent.height};

            binding_table bindingTable;

            ctx.bind_textures(bindingTable,
                {
                    {"t_InSource", inSource},
                    {"t_InMoments", inMoments},
                    {"t_OutFiltered", outFiltered},
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