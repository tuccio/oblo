#pragma once

#include <oblo/math/vec2.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/draw/render_pass_initializer.hpp>
#include <oblo/vulkan/graph/init_context.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    struct picking_configuration
    {
        bool enabled;
        vec2 coordinates;
        buffer resultBuffer;
        buffer downloadBuffer;
    };

    void add_pipeline_barrier_cmd(VkCommandBuffer commandBuffer,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImage image,
        VkFormat format,
        u32 layerCount,
        u32 mipLevels);

    struct forward_pass
    {
        data<picking_configuration> inPickingConfiguration;

        data<vec2u> inResolution;
        data<buffer_binding_table> inPerViewBindingTable;
        resource<texture> outRenderTarget;
        resource<texture> outPickingIdBuffer;
        resource<texture> outDepthBuffer;

        resource<buffer> pickingCoordinatesBuffer;
        resource<buffer> inPickingBuffer;

        h32<render_pass> renderPass;

        h32<string> pickingEnabledDefine;

        // Cached in build to avoid re-accessing all the time
        bool isPickingEnabled;

        void build(const runtime_builder& builder)
        {
            const auto resolution = builder.access(inResolution);

            builder.create(outRenderTarget,
                {
                    .width = resolution.x,
                    .height = resolution.y,
                    .format = VK_FORMAT_R8G8B8A8_UNORM,
                    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                },
                resource_usage::render_target_write);

            builder.create(outDepthBuffer,
                {
                    .width = resolution.x,
                    .height = resolution.y,
                    .format = VK_FORMAT_D24_UNORM_S8_UINT,
                    .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                },
                resource_usage::depth_stencil_write);

            const auto& pickingConfiguration = builder.access(inPickingConfiguration);
            isPickingEnabled = pickingConfiguration.enabled;

            if (isPickingEnabled)
            {
                builder.create(pickingCoordinatesBuffer,
                    {
                        .size = sizeof(vec2),
                        .data = std::as_bytes(std::span{&pickingConfiguration.coordinates, 1}),
                    });

                builder.create(outPickingIdBuffer,
                    {
                        .width = resolution.x,
                        .height = resolution.y,
                        .format = VK_FORMAT_R32_UINT,
                        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    },
                    resource_usage::render_target_write);
            }
        }

        void init(const init_context& context)
        {
            auto& renderPassManager = context.get_render_pass_manager();

            renderPass = renderPassManager.register_render_pass({
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

        void execute(const runtime_context& context)
        {
            const auto renderTarget = context.access(outRenderTarget);
            const auto depthBuffer = context.access(outDepthBuffer);

            auto& renderPassManager = context.get_render_pass_manager();

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

            small_vector<VkRenderingAttachmentInfo, 2> colorAttachments = {
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
                const auto pickingIdBuffer = context.access(outPickingIdBuffer);

                pipelineInitializer.defines = {&pickingEnabledDefine, 1};

                pipelineInitializer.renderTargets.colorAttachmentFormats.emplace_back(
                    pickingIdBuffer.initializer.format);

                colorAttachments.push_back({
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = pickingIdBuffer.view,
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                });
            }

            const auto pipeline = renderPassManager.get_or_create_pipeline(renderPass, pipelineInitializer);

            const VkCommandBuffer commandBuffer = context.get_command_buffer();

            render_pass_context renderPassContext{
                .commandBuffer = commandBuffer,
                .pipeline = pipeline,
            };

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

            buffer_binding_table nodeTable;

            if (isPickingEnabled)
            {
                auto& interner = context.get_string_interner();

                const buffer pickingCoordinates = context.access(pickingCoordinatesBuffer);
                const auto coordinates = interner.get_or_add("PickingCoordinatesBuffer");
                nodeTable.emplace(coordinates, pickingCoordinates);

                const auto* const cfg = context.access(inPickingConfiguration);

                const auto& resultBuffer = cfg->downloadBuffer;
                const auto result = interner.get_or_add("b_PickingResult");
                nodeTable.emplace(result, resultBuffer);

                // vkCmdFillBuffer(commandBuffer, resultBuffer.buffer, resultBuffer.offset, resultBuffer.size, 0);
            }

            if (renderPassManager.begin_rendering(renderPassContext, renderInfo))
            {
                const buffer_binding_table* bindingTables[] = {
                    context.access(inPerViewBindingTable),
                    &nodeTable,
                };

                renderPassManager.draw(renderPassContext,
                    context.get_resource_manager(),
                    context.get_draw_registry(),
                    bindingTables);

                renderPassManager.end_rendering(renderPassContext);

                if (isPickingEnabled)
                {
                    ///
                    const auto* cfg = context.access(inPickingConfiguration);
                    const auto pickingBuffer = context.access(outPickingIdBuffer);

                    const VkBufferImageCopy copyRegion{
                        .bufferOffset = cfg->downloadBuffer.offset,
                        .bufferRowLength = pickingBuffer.initializer.extent.width,
                        .bufferImageHeight = pickingBuffer.initializer.extent.height,
                        .imageSubresource =
                            {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .mipLevel = 0,
                                .baseArrayLayer = 0,
                                .layerCount = 1,
                            },
                        .imageOffset = {i32(cfg->coordinates.x), i32(cfg->coordinates.y), 0},
                        .imageExtent = {1, 1, 1},
                    };

                    // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                    // VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL

                    add_pipeline_barrier_cmd(commandBuffer,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        pickingBuffer.image,
                        pickingBuffer.initializer.format,
                        1,
                        1);

                    // VkImageMemoryBarrier imageMemoryBarrier = {
                    //     .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    //     .srcMask = VK_ACCESS_.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    //     .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    //     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    //     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    //     .image = pickingBuffer.image,
                    // };

                    // vkCmdPipelineBarrier(commandBuffer,
                    //     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    //     VK_PIPELINE_STAGE_TRANSFER_BIT,
                    //     0,
                    //     0,
                    //     nullptr,
                    //     0,
                    //     nullptr,
                    //     1,
                    //     &imageMemoryBarrier);

                    vkCmdCopyImageToBuffer(commandBuffer,
                        pickingBuffer.image,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        cfg->downloadBuffer.buffer,
                        1,
                        &copyRegion);

                    // const VkMemoryBarrier2 memoryBarrier{
                    //     .srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    //     .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
                    //     .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                    //     .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
                    // };

                    // const VkDependencyInfo dependency{
                    //     .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    //     .memoryBarrierCount = 1,
                    //     .pMemoryBarriers = &memoryBarrier,
                    // };

                    // vkCmdPipelineBarrier2(commandBuffer, &dependency);

                    ///

                    // const auto* cfg = context.access(inPickingConfiguration);

                    // const buffer& downloadBuffer = cfg->downloadBuffer;

                    // const VkBufferMemoryBarrier bufferMemoryBarrier{
                    //     .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    //     .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                    //     .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
                    //     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    //     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    //     .buffer = downloadBuffer.buffer,
                    //     .offset = downloadBuffer.offset,
                    //     .size = downloadBuffer.size,
                    // };

                    // vkCmdPipelineBarrier(commandBuffer,
                    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    //     VK_PIPELINE_STAGE_HOST_BIT,
                    //     0,
                    //     0,
                    //     nullptr,
                    //     1,
                    //     &bufferMemoryBarrier,
                    //     0,
                    //     nullptr);
                }
            }
        }
    };
}