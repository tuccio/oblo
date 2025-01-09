#include <oblo/vulkan/nodes/skybox_pass.hpp>

#include <oblo/resource/resource_ptr.hpp>
#include <oblo/scene/assets/texture.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    void skybox_pass::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        skyboxPass = passManager.register_compute_pass({
            .name = "Skybox Pass",
            .shaderSourcePath = "./vulkan/shaders/skybox/skybox.comp",
        });
    }

    void skybox_pass::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::compute);

        const auto resolution = ctx.access(inResolution);

        ctx.create(outShading,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT,
            },
            texture_usage::storage_write);
    }

    void skybox_pass::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_textures(bindingTable,
            {
                {"t_OutImage", outShading},
            });

        const auto commandBuffer = ctx.get_command_buffer();

        const auto pipeline = pm.get_or_create_pipeline(skyboxPass, {});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
        {
            const auto resolution = ctx.access(inResolution);

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            const auto skybox = ctx.access(inSkyboxResidentTexture);

            pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&skybox, 1}));

            vkCmdDispatch(ctx.get_command_buffer(),
                round_up_div(resolution.x, pm.get_subgroup_size()),
                resolution.y,
                1);

            pm.end_compute_pass(*pass);
        }
    }
}