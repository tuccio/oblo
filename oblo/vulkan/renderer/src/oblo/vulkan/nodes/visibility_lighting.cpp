#include <oblo/vulkan/nodes/visibility_lighting.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/data/light_visibility_event.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    void visibility_lighting::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        lightingPass = passManager.register_compute_pass({
            .name = "Lighting Pass",
            .shaderSourcePath = "./vulkan/shaders/visibility/visibility_lighting.comp",
        });

        ctx.set_pass_kind(pass_kind::compute);
    }

    void visibility_lighting::build(const frame_graph_build_context& ctx)
    {
        const auto resolution = ctx.access(inResolution);

        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        ctx.create(outShadedImage,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);
        ctx.acquire(inLightConfig, buffer_usage::uniform);
        ctx.acquire(inLightBuffer, buffer_usage::storage_read);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        if (ctx.has_source(inSurfelsGrid))
        {
            ctx.acquire(inSurfelsGrid, buffer_usage::storage_read);
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
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_buffers(bindingTable,
            {
                {"b_LightData", inLightBuffer},
                {"b_LightConfig", inLightConfig},
                {"b_InstanceTables", inInstanceTables},
                {"b_MeshTables", inMeshDatabase},
                {"b_CameraBuffer", inCameraBuffer},
                {"b_ShadowMaps", outShadowMaps},
            });

        ctx.bind_textures(bindingTable,
            {
                {"t_InVisibilityBuffer", inVisibilityBuffer},
                {"t_OutShadedImage", outShadedImage},
            });

        buffered_array<hashed_string_view, 1> defines;

        if (ctx.has_source(inSurfelsGrid))
        {
            ctx.bind_buffers(bindingTable,
                {
                    {"b_SurfelsGrid", inSurfelsGrid},
                });

            defines.emplace_back("SURFELS_GI");
        }

        const auto commandBuffer = ctx.get_command_buffer();

        const auto lightingPipeline = pm.get_or_create_pipeline(lightingPass, {.defines = defines});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, lightingPipeline))
        {
            const auto resolution = ctx.access(inResolution);

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            vkCmdDispatch(ctx.get_command_buffer(), round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);

            pm.end_compute_pass(*pass);
        }
    }

    void visibility_debug::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        albedoPass = passManager.register_compute_pass({
            .name = "Debug Pass",
            .shaderSourcePath = "./vulkan/shaders/visibility/visibility_debug.comp",
        });

        ctx.set_pass_kind(pass_kind::compute);
    }

    void visibility_debug::build(const frame_graph_build_context& ctx)
    {
        const auto resolution = ctx.access(inResolution);

        ctx.acquire(inVisibilityBuffer, texture_usage::storage_read);

        ctx.create(outShadedImage,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);

        ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

        acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);
    }

    void visibility_debug::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_buffers(bindingTable,
            {
                {"b_InstanceTables", inInstanceTables},
                {"b_MeshTables", inMeshDatabase},
                {"b_CameraBuffer", inCameraBuffer},
            });

        ctx.bind_textures(bindingTable,
            {
                {"t_InVisibilityBuffer", inVisibilityBuffer},
                {"t_OutShadedImage", outShadedImage},
            });

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

        const auto commandBuffer = ctx.get_command_buffer();

        const auto lightingPipeline = pm.get_or_create_pipeline(albedoPass, {.defines = {&define, 1}});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, lightingPipeline))
        {
            const auto resolution = ctx.access(inResolution);

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            vkCmdDispatch(ctx.get_command_buffer(), round_up_div(resolution.x, 8u), round_up_div(resolution.y, 8u), 1);

            pm.end_compute_pass(*pass);
        }
    }
}