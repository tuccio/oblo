#include <oblo/vulkan/nodes/postprocess/tone_mapping_node.hpp>

#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/texture.hpp>

namespace oblo::vk
{
    void tone_mapping_node::init(const frame_graph_init_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        toneMappingPass = pm.register_compute_pass({
            .name = "Tone-Mapping Pass",
            .shaderSourcePath = "./vulkan/shaders/tone_mapping/tone_mapping.comp",
        });
    }

    void tone_mapping_node::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::compute);

        const auto hdrInit = ctx.get_current_initializer(inHDR).value_or(image_initializer{});

        ctx.acquire(inHDR, texture_usage::storage_read);

        ctx.create(outLDR,
            texture_resource_initializer{
                .width = hdrInit.extent.width,
                .height = hdrInit.extent.height,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT,
            },
            texture_usage::storage_write);
    }

    void tone_mapping_node::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_textures(bindingTable,
            {
                {"t_InHDR", inHDR},
                {"t_OutLDR", outLDR},
            });

        const auto commandBuffer = ctx.get_command_buffer();

        const auto pipeline = pm.get_or_create_pipeline(toneMappingPass, {});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
        {
            const auto& extents = ctx.access(outLDR).initializer.extent;

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            vkCmdDispatch(ctx.get_command_buffer(),
                round_up_div(extents.width, pm.get_subgroup_size()),
                extents.height,
                1);

            pm.end_compute_pass(*pass);
        }
    }
}