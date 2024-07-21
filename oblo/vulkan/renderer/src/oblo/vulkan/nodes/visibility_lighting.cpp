#include <oblo/vulkan/nodes/visibility_lighting.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/data/light_visibility_event.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/nodes/frustum_culling.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    void visibility_lighting::init(const frame_graph_init_context& context)
    {
        auto& passManager = context.get_pass_manager();

        lightingPass = passManager.register_compute_pass({
            .name = "Lighting Pass",
            .shaderSourcePath = "./vulkan/shaders/visibility/visibility_lighting.comp",
        });
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
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, pass_kind::compute, buffer_usage::uniform);
        ctx.acquire(inLightConfig, pass_kind::compute, buffer_usage::uniform);
        ctx.acquire(inLightBuffer, pass_kind::compute, buffer_usage::storage_read);

        ctx.acquire(inMeshDatabase, pass_kind::compute, buffer_usage::storage_read);

        acquire_instance_tables(ctx,
            inInstanceTables,
            inInstanceBuffers,
            pass_kind::compute,
            buffer_usage::storage_read);

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
            pass_kind::compute,
            buffer_usage::uniform);
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

        const auto commandBuffer = ctx.get_command_buffer();

        const auto lightingPipeline = pm.get_or_create_pipeline(lightingPass, {});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, lightingPipeline))
        {
            const auto resolution = ctx.access(inResolution);

            pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&resolution, 1}));

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            vkCmdDispatch(ctx.get_command_buffer(), round_up_multiple(resolution.x, 64u), resolution.y, 1);

            pm.end_compute_pass(*pass);
        }
    }

    void visibility_debug::init(const frame_graph_init_context& context)
    {
        auto& passManager = context.get_pass_manager();

        albedoPass = passManager.register_compute_pass({
            .name = "Debug Pass",
            .shaderSourcePath = "./vulkan/shaders/visibility/visibility_debug.comp",
        });
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
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, pass_kind::compute, buffer_usage::uniform);

        ctx.acquire(inMeshDatabase, pass_kind::compute, buffer_usage::storage_read);

        acquire_instance_tables(ctx,
            inInstanceTables,
            inInstanceBuffers,
            pass_kind::compute,
            buffer_usage::storage_read);
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

        h32<string> define{};

        switch (ctx.access(inDebugMode))
        {
        case visibility_debug_mode::albedo:
            define = ctx.get_string_interner().get_or_add("OUT_ALBEDO");
            break;
        case visibility_debug_mode::normal_map:
            define = ctx.get_string_interner().get_or_add("OUT_NORMAL_MAP");
            break;
        case visibility_debug_mode::normals:
            define = ctx.get_string_interner().get_or_add("OUT_NORMALS");
            break;
        case visibility_debug_mode::tangents:
            define = ctx.get_string_interner().get_or_add("OUT_TANGENTS");
            break;
        case visibility_debug_mode::bitangents:
            define = ctx.get_string_interner().get_or_add("OUT_BITANGENTS");
            break;
        case visibility_debug_mode::uv0:
            define = ctx.get_string_interner().get_or_add("OUT_UV0");
            break;
        case visibility_debug_mode::meshlet:
            define = ctx.get_string_interner().get_or_add("OUT_MESHLET");
            break;
        case visibility_debug_mode::metalness:
            define = ctx.get_string_interner().get_or_add("OUT_METALNESS");
            break;
        case visibility_debug_mode::roughness:
            define = ctx.get_string_interner().get_or_add("OUT_ROUGHNESS");
            break;
        case visibility_debug_mode::emissive:
            define = ctx.get_string_interner().get_or_add("OUT_EMISSIVE");
            break;
        default:
            unreachable();
        }

        const auto commandBuffer = ctx.get_command_buffer();

        const auto lightingPipeline = pm.get_or_create_pipeline(albedoPass, {.defines = {&define, 1}});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, lightingPipeline))
        {
            const auto resolution = ctx.access(inResolution);

            pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&resolution, 1}));

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            vkCmdDispatch(ctx.get_command_buffer(), round_up_multiple(resolution.x, 64u), resolution.y, 1);

            pm.end_compute_pass(*pass);
        }
    }
}