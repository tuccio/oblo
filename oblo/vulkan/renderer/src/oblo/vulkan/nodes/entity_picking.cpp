#include <oblo/vulkan/nodes/entity_picking.hpp>

#include <oblo/core/utility.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/nodes/frustum_culling.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    void entity_picking::init(const frame_graph_init_context& context)
    {
        auto& passManager = context.get_pass_manager();

        pickingPass = passManager.register_compute_pass({
            .name = "Entity Picking Pass",
            .shaderSourcePath = "./vulkan/shaders/entity_picking/entity_picking.comp",
        });
    }

    void entity_picking::build(const frame_graph_build_context& ctx)
    {
        const auto& pickingConfiguration = ctx.access(inPickingConfiguration);

        skipExecution = !pickingConfiguration.enabled;

        if (skipExecution)
        {
            return;
        }

        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        acquire_instance_tables(ctx,
            inInstanceTables,
            inInstanceBuffers,
            pass_kind::compute,
            buffer_usage::storage_read);
    }

    void entity_picking::execute(const frame_graph_execute_context& ctx)
    {
        if (skipExecution)
        {
            return;
        }

        auto& pm = ctx.get_pass_manager();
        const auto& pickingConfiguration = ctx.access(inPickingConfiguration);

        binding_table bindingTable;

        ctx.bind_textures(bindingTable,
            {
                {"t_InVisibilityBuffer", inVisibilityBuffer},
            });

        ctx.bind_buffers(bindingTable,
            {
                {"b_InstanceTables", inInstanceTables},
            });

        bindingTable.emplace(ctx.get_string_interner().get_or_add("b_OutPickingId"),
            make_bindable_object(pickingConfiguration.outputBuffer));

        const auto commandBuffer = ctx.get_command_buffer();

        const auto lightingPipeline = pm.get_or_create_pipeline(pickingPass, {});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, lightingPipeline))
        {
            const vec2u screenPosition{u32(pickingConfiguration.coordinates.x + .5f),
                u32(pickingConfiguration.coordinates.y + .5f)};

            pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&screenPosition, 1}));

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            vkCmdDispatch(ctx.get_command_buffer(), 1, 1, 1);

            pm.end_compute_pass(*pass);
        }
    }
}