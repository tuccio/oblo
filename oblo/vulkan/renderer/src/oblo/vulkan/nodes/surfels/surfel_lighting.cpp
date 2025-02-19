#include <oblo/vulkan/nodes/surfels/surfel_lighting.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    namespace
    {
        constexpr u32 g_FilterPassCount = 3;
    }

    void surfel_lighting::init(const frame_graph_init_context& ctx)
    {
        lightingPass = ctx.register_compute_pass({
            .name = "Surfel Lighting Pass",
            .shaderSourcePath = "./vulkan/shaders/surfels/lighting.comp",
        });

        filterPass = ctx.register_compute_pass({
            .name = "Surfel Lighting Filter Pass",
            .shaderSourcePath = "./vulkan/shaders/surfels/gi_filter.comp",
        });
    }

    void surfel_lighting::build(const frame_graph_build_context& ctx)
    {
        lightingPassInstance = ctx.compute_pass(lightingPass, {});

        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        const auto visibilityBufferInit = ctx.get_current_initializer(inVisibilityBuffer).value_or({});

        const texture_resource_initializer outBufferInit{
            .width = visibilityBufferInit.extent.width,
            .height = visibilityBufferInit.extent.height,
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT,
        };

        ctx.create(filterAux, outBufferInit, texture_usage::storage_write);

        ctx.acquire(inSurfelsGrid, buffer_usage::storage_read);
        ctx.acquire(inSurfelsGridData, buffer_usage::storage_read);
        ctx.acquire(inSurfelsData, buffer_usage::storage_read);
        ctx.acquire(inSurfelsLightingData, buffer_usage::storage_read);
        ctx.acquire(inOutSurfelsLastUsage, buffer_usage::storage_write);

        acquire_view_buffers(ctx);

        resource<texture> filterInOut[] = {filterAux, outIndirectLighting};

        filterPassInstances = allocate_n_span<h32<compute_pass_instance>>(ctx.get_frame_allocator(), g_FilterPassCount);

        string_builder builder;

        for (u32 passIndex = 0; passIndex < g_FilterPassCount; ++passIndex)
        {
            builder.clear().format("GI_FILTER_PASS_INDEX {}", passIndex);
            const hashed_string_view defines[] = {builder.as<hashed_string_view>()};

            filterPassInstances[passIndex] = ctx.compute_pass(filterPass, {.defines = defines});
            acquire_view_buffers(ctx);

            const auto in = filterInOut[passIndex % 2];
            const auto out = filterInOut[1 - passIndex % 2];

            ctx.acquire(in, texture_usage::storage_read);

            if (passIndex == 0)
            {
                ctx.create(out, outBufferInit, texture_usage::storage_write);
            }
            else
            {
                ctx.acquire(out, texture_usage::storage_write);
            }
        }
    }

    void surfel_lighting::execute(const frame_graph_execute_context& ctx)
    {
        binding_table bindingTable;

        bindingTable.bind_buffers({
            {"b_InstanceTables"_hsv, inInstanceTables},
            {"b_MeshTables"_hsv, inMeshDatabase},
            {"b_CameraBuffer"_hsv, inCameraBuffer},
            {"b_SurfelsGrid"_hsv, inSurfelsGrid},
            {"b_SurfelsGridData"_hsv, inSurfelsGridData},
            {"b_SurfelsData"_hsv, inSurfelsData},
            {"b_InSurfelsLighting"_hsv, inSurfelsLightingData},
            {"b_SurfelsLastUsage"_hsv, inOutSurfelsLastUsage},
        });

        bindingTable.bind_textures({
            {"t_InVisibilityBuffer"_hsv, inVisibilityBuffer},
            {"t_OutShadedImage"_hsv, filterAux},
        });

        const auto resolution = ctx.get_resolution(inVisibilityBuffer);

        if (const auto pass = ctx.begin_pass(lightingPassInstance))
        {
            ctx.bind_descriptor_sets(bindingTable);
            ctx.dispatch_compute(round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);
            ctx.end_pass();
        }

        bindingTable.remove("t_OutShadedImage"_hsv);

        for (u32 passIndex = 0; passIndex < g_FilterPassCount; ++passIndex)
        {
            const auto filterPassInstance = filterPassInstances[passIndex];

            if (ctx.begin_pass(filterPassInstance))
            {
                resource<texture> filterInOut[] = {filterAux, outIndirectLighting};

                const auto in = filterInOut[passIndex % 2];
                const auto out = filterInOut[1 - passIndex % 2];

                bindingTable.remove("t_InSource"_hsv);
                bindingTable.remove("t_OutFiltered"_hsv);

                bindingTable.bind_textures({
                    {"t_InSource"_hsv, in},
                    {"t_OutFiltered"_hsv, out},
                });

                ctx.bind_descriptor_sets(bindingTable);
                ctx.dispatch_compute(round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);
                ctx.end_pass();
            }
        }
    }

    void surfel_lighting::acquire_view_buffers(const frame_graph_build_context& ctx) const
    {
        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);
        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }
}