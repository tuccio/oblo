#include <oblo/vulkan/nodes/forward_pass.hpp>

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/draw/render_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/nodes/frustum_culling.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    namespace
    {
        struct forward_pass_push_constants
        {
            u32 instanceTableId;
        };
    }

    void forward_pass::init(const frame_graph_init_context& context)
    {
        auto& passManager = context.get_pass_manager();

        renderPass = passManager.register_render_pass({
            .name = "Forward Pass",
            .stages =
                {
                    {
                        .stage = pipeline_stages::vertex,
                        .shaderSourcePath = "./vulkan/shaders/forward/forward.vert",
                    },
                    {
                        .stage = pipeline_stages::fragment,
                        .shaderSourcePath = "./vulkan/shaders/forward/forward.frag",
                    },
                },
        });

        pickingEnabledDefine = context.get_string_interner().get_or_add("OBLO_PICKING_ENABLED");
    }

    void forward_pass::build(const frame_graph_build_context& ctx)
    {
        const auto resolution = ctx.access(inResolution);

        ctx.create(outRenderTarget,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            },
            texture_usage::render_target_write);

        ctx.create(outDepthBuffer,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = VK_FORMAT_D24_UNORM_S8_UINT,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            },
            texture_usage::depth_stencil_write);

        ctx.acquire(inLightConfig, pass_kind::graphics, buffer_usage::uniform);
        ctx.acquire(inLightData, pass_kind::graphics, buffer_usage::storage_read);

        const auto& pickingConfiguration = ctx.access(inPickingConfiguration);
        isPickingEnabled = pickingConfiguration.enabled;

        if (isPickingEnabled)
        {
            ctx.create(outPickingIdBuffer,
                {
                    .width = resolution.x,
                    .height = resolution.y,
                    .format = VK_FORMAT_R32_UINT,
                    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                },
                texture_usage::render_target_write);
        }

        for (const auto& drawData : ctx.access(inDrawData))
        {
            ctx.acquire(drawData.drawCallCountBuffer, pass_kind::graphics, buffer_usage::indirect);
            ctx.acquire(drawData.preCullingIdMap, pass_kind::graphics, buffer_usage::storage_read);
        }

        for (const auto& drawCallBuffer : ctx.access(inDrawCallBuffer))
        {
            ctx.acquire(drawCallBuffer, pass_kind::graphics, buffer_usage::indirect);
        }

        acquire_instance_tables(ctx,
            inInstanceTables,
            inInstanceBuffers,
            pass_kind::graphics,
            buffer_usage::storage_read);
    }

    void forward_pass::execute(const frame_graph_execute_context& ctx)
    {
        const std::span drawData = ctx.access(inDrawData);

        if (drawData.empty())
        {
            return;
        }

        const auto renderTarget = ctx.access(outRenderTarget);
        const auto depthBuffer = ctx.access(outDepthBuffer);

        auto& passManager = ctx.get_pass_manager();

        render_pipeline_initializer pipelineInitializer{
            .renderTargets =
                {
                    .colorAttachmentFormats = {renderTarget.initializer.format},
                    .depthFormat = VK_FORMAT_D24_UNORM_S8_UINT,
                },
            .depthStencilState =
                {
                    .depthTestEnable = true,
                    .depthWriteEnable = true,
                    .depthCompareOp = VK_COMPARE_OP_GREATER, // We use reverse depth
                },
            .rasterizationState =
                {
                    .polygonMode = VK_POLYGON_MODE_FILL,
                    .cullMode = VK_CULL_MODE_NONE,
                    .lineWidth = 1.f,
                },
        };

        buffered_array<VkRenderingAttachmentInfo, 2> colorAttachments = {
            {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = renderTarget.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            },
        };

        if (isPickingEnabled)
        {
            const auto pickingIdBuffer = ctx.access(outPickingIdBuffer);

            pipelineInitializer.defines = {&pickingEnabledDefine, 1};

            pipelineInitializer.renderTargets.colorAttachmentFormats.emplace_back(pickingIdBuffer.initializer.format);

            colorAttachments.push_back({
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = pickingIdBuffer.view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            });
        }

        const auto pipeline = passManager.get_or_create_pipeline(renderPass, pipelineInitializer);

        const VkCommandBuffer commandBuffer = ctx.get_command_buffer();

        const VkRenderingAttachmentInfo depthAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = depthBuffer.view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        const auto [renderWidth, renderHeight, _] = renderTarget.initializer.extent;

        const VkRenderingInfo renderInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {.extent{.width = renderWidth, .height = renderHeight}},
            .layerCount = 1,
            .colorAttachmentCount = u32(colorAttachments.size()),
            .pColorAttachments = colorAttachments.data(),
            .pDepthAttachment = &depthAttachment,
        };

        setup_viewport_scissor(commandBuffer, renderWidth, renderHeight);

        buffer_binding_table perDrawBindingTable;
        buffer_binding_table passBindingTable;

        ctx.add_bindings(passBindingTable,
            {
                {inLightData, "b_LightData"},
                {inLightConfig, "b_LightConfig"},
                {inInstanceTables, "b_InstanceTables"},
            });

        const buffer_binding_table* bindingTables[] = {
            &perDrawBindingTable,
            &passBindingTable,
            &ctx.access(inPerViewBindingTable),
        };

        if (const auto pass = passManager.begin_render_pass(commandBuffer, pipeline, renderInfo))
        {
            const auto drawCallBufferSpan = ctx.access(inDrawCallBuffer);

            for (usize drawCallIndex = 0; drawCallIndex < drawData.size(); ++drawCallIndex)
            {
                const auto& culledDraw = drawData[drawCallIndex];

                const auto drawCallBuffer = ctx.access(drawCallBufferSpan[drawCallIndex]);
                const auto drawCallCountBuffer = ctx.access(culledDraw.drawCallCountBuffer);

                perDrawBindingTable.clear();

                ctx.add_bindings(perDrawBindingTable,
                    {
                        {culledDraw.preCullingIdMap, "b_PreCullingIdMap"},
                    });

                const forward_pass_push_constants pushConstants{
                    .instanceTableId = culledDraw.sourceData.instanceTableId,
                };

                passManager.bind_descriptor_sets(*pass, bindingTables);
                passManager.push_constants(*pass,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    as_bytes(std::span{&pushConstants, 1}));

                if (culledDraw.sourceData.drawCommands.isIndexed)
                {
                    OBLO_ASSERT(drawCallBuffer.size ==
                        culledDraw.sourceData.drawCommands.drawCount * sizeof(VkDrawIndexedIndirectCommand));

                    vkCmdBindIndexBuffer(commandBuffer,
                        culledDraw.sourceData.drawCommands.indexBuffer,
                        culledDraw.sourceData.drawCommands.indexBufferOffset,
                        culledDraw.sourceData.drawCommands.indexType);

                    vkCmdDrawIndexedIndirectCount(commandBuffer,
                        drawCallBuffer.buffer,
                        drawCallBuffer.offset,
                        drawCallCountBuffer.buffer,
                        drawCallCountBuffer.offset,
                        culledDraw.sourceData.drawCommands.drawCount,
                        sizeof(VkDrawIndexedIndirectCommand));
                }
                else
                {
                    OBLO_ASSERT(drawCallBuffer.size ==
                        culledDraw.sourceData.drawCommands.drawCount * sizeof(VkDrawIndirectCommand));

                    vkCmdDrawIndirectCount(commandBuffer,
                        drawCallBuffer.buffer,
                        drawCallBuffer.offset,
                        drawCallCountBuffer.buffer,
                        drawCallCountBuffer.offset,
                        culledDraw.sourceData.drawCommands.drawCount,
                        sizeof(VkDrawIndirectCommand));
                }
            }

            passManager.end_render_pass(*pass);
        }
    }
}