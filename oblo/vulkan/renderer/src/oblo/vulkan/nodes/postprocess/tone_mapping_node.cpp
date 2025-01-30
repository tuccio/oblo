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
        toneMappingPass = ctx.register_compute_pass({
            .name = "Tone-Mapping Pass",
            .shaderSourcePath = "./vulkan/shaders/tone_mapping/tone_mapping.comp",
        });
    }

    void tone_mapping_node::build(const frame_graph_build_context& ctx)
    {
        toneMappingPassInstance = ctx.compute_pass(toneMappingPass, {});

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
        binding_table bindingTable;

        bindingTable.bind_textures({
            {"t_InHDR"_hsv, inHDR},
            {"t_OutLDR"_hsv, outLDR},
        });

        if (const auto pass = ctx.begin_pass(toneMappingPassInstance))
        {
            const auto& extents = ctx.access(outLDR).initializer.extent;

            ctx.bind_descriptor_sets(bindingTable);

            ctx.dispatch_compute(round_up_div(extents.width, ctx.get_gpu_info().subgroupSize), extents.height, 1);

            ctx.end_pass();
        }
    }
}