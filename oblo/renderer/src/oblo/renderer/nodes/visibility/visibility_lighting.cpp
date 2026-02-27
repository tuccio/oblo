#include <oblo/renderer/nodes/visibility/visibility_lighting.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/data/draw_buffer_data.hpp>
#include <oblo/renderer/data/light_visibility_event.hpp>
#include <oblo/renderer/data/picking_configuration.hpp>
#include <oblo/renderer/draw/compute_pass_initializer.hpp>
#include <oblo/renderer/graph/node_common.hpp>

namespace oblo
{
    void visibility_lighting::init(const frame_graph_init_context& ctx)
    {
        lightingPass = ctx.register_compute_pass({
            .name = "Lighting Pass",
            .shaderSourcePath = "./vulkan/shaders/visibility/visibility_lighting.comp",
        });
    }

    void visibility_lighting::build(const frame_graph_build_context& ctx)
    {
        const bool withGI = ctx.has_source(inSurfelsGrid);

        buffered_array<hashed_string_view, 1> defines;

        if (withGI)
        {
            defines.emplace_back("SURFELS_GI"_hsv);
        }

        lightingPassInstance = ctx.compute_pass(lightingPass, {.defines = defines});

        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        const auto resolution = ctx.access(inResolution);

        ctx.create(outShadedImage,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = gpu::image_format::r16g16b16a16_sfloat,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inLightConfig, buffer_usage::uniform);
        ctx.acquire(inLightBuffer, buffer_usage::storage_read);
        ctx.acquire(inSkyboxSettingsBuffer, buffer_usage::uniform);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        if (withGI)
        {
            ctx.acquire(inSurfelsGrid, buffer_usage::storage_read);
            ctx.acquire(inSurfelsGridData, buffer_usage::storage_read);
            ctx.acquire(inSurfelsData, buffer_usage::storage_read);
            ctx.acquire(inSurfelsLightingData, buffer_usage::storage_read);
            ctx.acquire(inOutSurfelsLastUsage, buffer_usage::storage_write);

            ctx.acquire(inAmbientOcclusion, texture_usage::shader_read);
        }

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

        const auto lights = ctx.access(inLights);

        auto shadowMaps = allocate_n_span<h32<resident_texture>>(ctx.get_frame_allocator(), lights.size());
        std::uninitialized_value_construct_n(shadowMaps.data(), shadowMaps.size());

        // We register all shadow maps
        for (const auto& e : ctx.access(inShadowSink))
        {
            shadowMaps[e.lightIndex] = ctx.acquire_bindless(e.resource, texture_usage::shader_read);
        }

        ctx.create(outShadowMaps,
            {
                .size = narrow_cast<u32>(shadowMaps.size_bytes()),
                .data = as_bytes(shadowMaps),
            },
            buffer_usage::storage_read);
    }

    void visibility_lighting::execute(const frame_graph_execute_context& ctx)
    {
        binding_table bindingTable;

        bindingTable.bind_buffers({
            {"b_LightData"_hsv, inLightBuffer},
            {"b_LightConfig"_hsv, inLightConfig},
            {"b_InstanceTables"_hsv, inInstanceTables},
            {"b_MeshTables"_hsv, inMeshDatabase},
            {"b_CameraBuffer"_hsv, inCameraBuffer},
            {"b_ShadowMaps"_hsv, outShadowMaps},
            {"b_SkyboxSettings"_hsv, inSkyboxSettingsBuffer},
        });

        bindingTable.bind_textures({
            {"t_InVisibilityBuffer"_hsv, inVisibilityBuffer},
            {"t_OutShadedImage"_hsv, outShadedImage},
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

            bindingTable.bind_textures({
                {"t_InAmbientOcclusion"_hsv, inAmbientOcclusion},
            });
        }

        if (const auto pass = ctx.begin_pass(lightingPassInstance))
        {
            const auto resolution = ctx.access(inResolution);

            ctx.bind_descriptor_sets(bindingTable);

            ctx.dispatch_compute(round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);

            ctx.end_pass();
        }
    }

    void visibility_debug::init(const frame_graph_init_context& ctx)
    {
        debugPass = ctx.register_compute_pass({
            .name = "Debug Pass",
            .shaderSourcePath = "./vulkan/shaders/visibility/visibility_debug.comp",
        });
    }

    void visibility_debug::build(const frame_graph_build_context& ctx)
    {
        hashed_string_view define{};

        const auto debugMode = ctx.access(inDebugMode);

        switch (debugMode)
        {
        case visibility_debug_mode::albedo:
            define = "OUT_ALBEDO"_hsv;
            break;
        case visibility_debug_mode::normal_map:
            define = "OUT_NORMAL_MAP"_hsv;
            break;
        case visibility_debug_mode::normals:
            define = "OUT_NORMALS"_hsv;
            break;
        case visibility_debug_mode::tangents:
            define = "OUT_TANGENTS"_hsv;
            break;
        case visibility_debug_mode::bitangents:
            define = "OUT_BITANGENTS"_hsv;
            break;
        case visibility_debug_mode::uv0:
            define = "OUT_UV0"_hsv;
            break;
        case visibility_debug_mode::meshlet:
            define = "OUT_MESHLET"_hsv;
            break;
        case visibility_debug_mode::metalness:
            define = "OUT_METALNESS"_hsv;
            break;
        case visibility_debug_mode::roughness:
            define = "OUT_ROUGHNESS"_hsv;
            break;
        case visibility_debug_mode::emissive:
            define = "OUT_EMISSIVE"_hsv;
            break;
        case visibility_debug_mode::motion_vectors:
            define = "OUT_MOTION_VECTORS"_hsv;
            break;
        default:
            unreachable();
        }

        OBLO_ASSERT(!define.empty());

        debugPassInstance = ctx.compute_pass(debugPass, {.defines = {&define, 1}});

        const auto resolution = ctx.access(inResolution);

        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        ctx.create(outShadedImage,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = gpu::image_format::r8g8b8a8_unorm,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void visibility_debug::execute(const frame_graph_execute_context& ctx)
    {
        binding_table bindingTable;

        bindingTable.bind_buffers({
            {"b_InstanceTables"_hsv, inInstanceTables},
            {"b_MeshTables"_hsv, inMeshDatabase},
            {"b_CameraBuffer"_hsv, inCameraBuffer},
        });

        bindingTable.bind_textures({
            {"t_InVisibilityBuffer"_hsv, inVisibilityBuffer},
            {"t_OutShadedImage"_hsv, outShadedImage},
        });

        if (const auto pass = ctx.begin_pass(debugPassInstance))
        {
            const auto resolution = ctx.access(inResolution);

            ctx.bind_descriptor_sets(bindingTable);

            ctx.dispatch_compute(round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);

            ctx.end_pass();
        }
    }
}