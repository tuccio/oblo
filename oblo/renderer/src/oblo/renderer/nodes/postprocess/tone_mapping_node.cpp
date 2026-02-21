#include <oblo/vulkan/nodes/postprocess/tone_mapping_node.hpp>

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/gpu_temporary_aliases.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/texture.hpp>

namespace oblo
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

        const auto hdrInit = ctx.get_current_initializer(inHDR).value_or(texture_init_desc{});

        ctx.acquire(inHDR, texture_usage::storage_read);

        ctx.create(outLDR,
            texture_resource_initializer{
                .width = hdrInit.width,
                .height = hdrInit.height,
                .format = texture_format::r8g8b8a8_unorm,
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
            const vec2u resolution = ctx.get_resolution(outLDR);

            ctx.bind_descriptor_sets(bindingTable);

            ctx.dispatch_compute(round_up_div(resolution.x, ctx.get_gpu_info().subgroupSize), resolution.y, 1);

            ctx.end_pass();
        }
    }
}