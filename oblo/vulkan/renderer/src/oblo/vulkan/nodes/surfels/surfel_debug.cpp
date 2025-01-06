#include <oblo/vulkan/nodes/surfels/surfel_debug.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/math/constants.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/draw/render_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    namespace
    {
        void generate_sphere_geometry(dynamic_array<f32>& vertices, f32 radius, u32 sectorCount, u32 stackCount)
        {
            const u32 expectedSize = 3u * (stackCount + 1) * (sectorCount + 1);

            vertices.reserve(expectedSize);

            const f32 sectorStep = 2 * pi / sectorCount;
            const f32 stackStep = pi / stackCount;

            for (u32 i = 0; i <= stackCount; ++i)
            {
                const f32 stackAngle = pi / 2 - i * stackStep; // starting from pi/2 to -pi/2

                const f32 xy = radius * std::cosf(stackAngle); // r * cos(u)
                const f32 z = radius * std::sinf(stackAngle);  // r * sin(u)

                // add (sectorCount+1) vertices per stack
                // first and last vertices have same position and normal, but different tex coords
                for (u32 j = 0; j <= sectorCount; ++j)
                {
                    const f32 sectorAngle = j * sectorStep; // starting from 0 to 2pi

                    // vertex position (x, y, z)
                    const f32 x = xy * cosf(sectorAngle); // r * cos(u) * cos(v)
                    const f32 y = xy * sinf(sectorAngle); // r * cos(u) * sin(v)

                    vertices.push_back(x);
                    vertices.push_back(y);
                    vertices.push_back(z);
                }
            }

            OBLO_ASSERT(vertices.size() == expectedSize);
        }
    }

    void surfel_debug::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        debugPass = passManager.register_render_pass({
            .name = "GI Debug Pass",
            .stages =
                {
                    {
                        .stage = pipeline_stages::vertex,
                        .shaderSourcePath = "./vulkan/shaders/surfels/surfel_debug_view.vert",
                    },
                    {
                        .stage = pipeline_stages::fragment,
                        .shaderSourcePath = "./vulkan/shaders/surfels/surfel_debug_view.frag",
                    },
                },
        });

        generate_sphere_geometry(sphereGeometryData, 1.f, 25, 25);
    }

    void surfel_debug::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::graphics);

        ctx.acquire(inCameraBuffer, buffer_usage::uniform);

        ctx.acquire(inOutImage, texture_usage::render_target_write);
        ctx.acquire(inDepthBuffer, texture_usage::depth_stencil_read);

        ctx.acquire(inSurfelsData, buffer_usage::storage_read);
        ctx.acquire(inSurfelsGrid, buffer_usage::storage_read);

        ctx.create(sphereGeometry,
            buffer_resource_initializer{
                .size = u32(sphereGeometryData.size_bytes()),
                .data = as_bytes(std::span{sphereGeometryData}),
            },
            buffer_usage::storage_read);
    }

    void surfel_debug::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        const auto image = ctx.access(inOutImage);
        const auto depth = ctx.access(inDepthBuffer);

        const render_pipeline_initializer pipelineInitializer{
            .renderTargets =
                {
                    .colorAttachmentFormats = {image.initializer.format},
                    .depthFormat = depth.initializer.format,
                },
            .depthStencilState =
                {
                    .depthTestEnable = true,
                    .depthWriteEnable = false,
                    .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
                },
            .rasterizationState =
                {
                    .polygonMode = VK_POLYGON_MODE_FILL,
                    .cullMode = VK_CULL_MODE_NONE,
                    .lineWidth = 1.f,
                },
            .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
        };

        const VkRenderingAttachmentInfo colorAttachments[] = {
            {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = image.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            },
        };

        const VkRenderingAttachmentInfo depthAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = depth.view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_NONE,
        };

        const auto [renderWidth, renderHeight, _] = image.initializer.extent;

        const VkRenderingInfo renderInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea =
                {
                    .extent =
                        {
                            .width = renderWidth,
                            .height = renderHeight,
                        },
                },
            .layerCount = 1,
            .colorAttachmentCount = array_size(colorAttachments),
            .pColorAttachments = colorAttachments,
            .pDepthAttachment = &depthAttachment,
        };

        const auto pipeline = pm.get_or_create_pipeline(debugPass, pipelineInitializer);

        const VkCommandBuffer commandBuffer = ctx.get_command_buffer();

        setup_viewport_scissor(commandBuffer, renderWidth, renderHeight);

        if (const auto pass = pm.begin_render_pass(commandBuffer, pipeline, renderInfo))
        {
            binding_table bindingTable;

            ctx.bind_buffers(bindingTable,
                {
                    {"b_CameraBuffer", inCameraBuffer},
                    {"b_SurfelsData", inSurfelsData},
                    {"b_SurfelsGrid", inSurfelsGrid},
                    {"b_SphereGeometry", sphereGeometry},
                });

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            const u32 vertexCount = u32(sphereGeometryData.size() / 3);
            constexpr u32 instanceCount = 1 << 16u;
            vkCmdDraw(commandBuffer, vertexCount, instanceCount, 0, 0);

            pm.end_render_pass(*pass);
        }
    }

    void surfel_debug_tile_coverage::init(const frame_graph_init_context& ctx)
    {
        auto& passManager = ctx.get_pass_manager();

        debugPass = passManager.register_compute_pass({
            .name = "GI Debug Tile Coverage Pass",
            .shaderSourcePath = "./vulkan/shaders/surfels/surfel_debug_tile_coverage.comp",
        });
    }

    void surfel_debug_tile_coverage::build(const frame_graph_build_context& ctx)
    {
        const auto resolution = ctx.access(inResolution);

        ctx.begin_pass(pass_kind::compute);

        ctx.acquire(inTileCoverage, buffer_usage::storage_read);

        ctx.create(outImage,
            texture_resource_initializer{
                .width = resolution.x,
                .height = resolution.y,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT,
            },
            texture_usage::storage_write);
    }

    void surfel_debug_tile_coverage::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_buffers(bindingTable,
            {
                {"b_InTileCoverage", inTileCoverage},
            });

        ctx.bind_textures(bindingTable,
            {
                {"t_OutImage", outImage},
            });

        const auto commandBuffer = ctx.get_command_buffer();

        const auto pipeline = pm.get_or_create_pipeline(debugPass, {});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
        {
            const auto resolution = ctx.access(inResolution);

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&resolution, 1}));

            vkCmdDispatch(ctx.get_command_buffer(),
                round_up_div(resolution.x, pm.get_subgroup_size()),
                round_up_div(resolution.y, pm.get_subgroup_size()),
                1);

            pm.end_compute_pass(*pass);
        }
    }
}