#include <oblo/vulkan/nodes/surfels/surfel_lighting.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    void surfel_lighting::init(const frame_graph_init_context& ctx)
    {
        lightingPass = ctx.register_compute_pass({
            .name = "Lighting Pass",
            .shaderSourcePath = "./vulkan/shaders/surfels/lighting.comp",
        });
    }

    void surfel_lighting::build(const frame_graph_build_context& ctx)
    {
        lightingPassInstance = ctx.compute_pass(lightingPass, {});

        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        const auto visibilityBufferInit = ctx.get_current_initializer(inVisibilityBuffer).value_or({});

        ctx.create(outIndirectLighting,
            {
                .width = visibilityBufferInit.extent.width,
                .height = visibilityBufferInit.extent.height,
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        ctx.acquire(inSurfelsGrid, buffer_usage::storage_read);
        ctx.acquire(inSurfelsGridData, buffer_usage::storage_read);
        ctx.acquire(inSurfelsData, buffer_usage::storage_read);
        ctx.acquire(inSurfelsLightingData, buffer_usage::storage_read);
        ctx.acquire(inOutSurfelsLastUsage, buffer_usage::storage_write);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void surfel_lighting::execute(const frame_graph_execute_context& ctx)
    {
        binding_table bindingTable;

        bindingTable.bind_buffers({
            {"b_InstanceTables"_hsv, inInstanceTables},
            {"b_MeshTables"_hsv, inMeshDatabase},
            {"b_CameraBuffer"_hsv, inCameraBuffer},
        });

        bindingTable.bind_textures({
            {"t_InVisibilityBuffer"_hsv, inVisibilityBuffer},
            {"t_OutShadedImage"_hsv, outIndirectLighting},
        });

        if (ctx.has_source(inSurfelsGrid))
        {
            bindingTable.bind_buffers({
                {"b_SurfelsGrid"_hsv, inSurfelsGrid},
                {"b_SurfelsGridData"_hsv, inSurfelsGridData},
                {"b_SurfelsData"_hsv, inSurfelsData},
                {"b_InSurfelsLighting"_hsv, inSurfelsLightingData},
                {"b_SurfelsLastUsage"_hsv, inOutSurfelsLastUsage},
            });
        }

        if (const auto pass = ctx.begin_pass(lightingPassInstance))
        {
            const auto resolution = ctx.get_resolution(inVisibilityBuffer);

            ctx.bind_descriptor_sets(bindingTable);

            ctx.dispatch_compute(round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);

            ctx.end_pass();
        }
    }
}