#include <oblo/renderer/nodes/utility/entity_picking.hpp>

#include <oblo/core/utility.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/data/draw_buffer_data.hpp>
#include <oblo/renderer/data/picking_configuration.hpp>
#include <oblo/renderer/draw/binding_table.hpp>
#include <oblo/renderer/draw/compute_pass_initializer.hpp>
#include <oblo/renderer/graph/node_common.hpp>
#include <oblo/renderer/utility.hpp>

namespace oblo
{
    void entity_picking::init(const frame_graph_init_context& ctx)
    {
        pickingPass = ctx.register_compute_pass({
            .name = "Entity Picking Pass",
            .shaderSourcePath = "./vulkan/shaders/entity_picking/entity_picking.comp",
        });
    }

    void entity_picking::build(const frame_graph_build_context& ctx)
    {
        pickingPassInstance = ctx.compute_pass(pickingPass, {});
        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        ctx.create(outPickingId,
            {
                .size = u32(sizeof(u32)),
            },
            buffer_usage::storage_write);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

        downloadInstance = ctx.transfer_pass();
        ctx.acquire(outPickingId, buffer_usage::download);
    }

    void entity_picking::execute(const frame_graph_execute_context& ctx)
    {
        const auto& pickingConfiguration = ctx.access(inPickingConfiguration);

        binding_table bindingTable;

        bindingTable.bind_textures({
            {"t_InVisibilityBuffer"_hsv, inVisibilityBuffer},
        });

        bindingTable.bind_buffers({
            {"b_InstanceTables"_hsv, inInstanceTables},
            {"b_OutPickingId"_hsv, outPickingId},
        });

        if (ctx.begin_pass(pickingPassInstance))
        {
            const vec2u screenPosition{u32(pickingConfiguration.coordinates.x + .5f),
                u32(pickingConfiguration.coordinates.y + .5f)};

            ctx.push_constants(shader_stage::compute, 0, as_bytes(std::span{&screenPosition, 1}));

            ctx.bind_descriptor_sets(bindingTable);

            ctx.dispatch_compute(1, 1, 1);

            ctx.end_pass();
        }
        else
        {
            return;
        }

        if (ctx.begin_pass(downloadInstance))
        {
            auto& pickingResult = ctx.access(outPickingResult);
            pickingResult = ctx.download(outPickingId);
            ctx.end_pass();
        }
    }
}