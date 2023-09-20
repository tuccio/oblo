#include <hello_world/hello_world.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/sandbox/context.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/shader_compiler.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

#include <imgui.h>

namespace oblo::vk
{
    bool hello_world::init(const sandbox_init_context& context)
    {
        const auto device = context.vkContext->get_device();
        return create_shader_modules(*context.frameAllocator, device) &&
               create_graphics_pipeline(device, context.swapchainFormat) &&
               create_vertex_buffers(context.vkContext->get_allocator());
    }

    void hello_world::shutdown(const sandbox_shutdown_context& context)
    {
        auto& allocator = context.vkContext->get_allocator();

        allocator.destroy(m_positions);
        allocator.destroy(m_colors);

        reset_device_objects(context.vkContext->get_device(),
                             m_graphicsPipeline,
                             m_pipelineLayout,
                             m_vertShaderModule,
                             m_fragShaderModule);
    }

    void hello_world::update(const sandbox_render_context& context)
    {
        auto& vkContext = *context.vkContext;
        auto& resourceManager = vkContext.get_resource_manager();

        auto& commandBuffer = vkContext.get_active_command_buffer();

        {
            const auto swapchainTexture = resourceManager.get(context.swapchainTexture);

            const VkImageMemoryBarrier imageMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .image = swapchainTexture.image,
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
            };

            vkCmdPipelineBarrier(commandBuffer.get(),
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &imageMemoryBarrier);
        }

        const auto& swapchainTexture = resourceManager.get(context.swapchainTexture);

        const VkRenderingAttachmentInfo colorAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = swapchainTexture.view,
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        const VkRenderingInfo renderInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {.extent{.width = context.width, .height = context.height}},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
        };

        vkCmdBindPipeline(commandBuffer.get(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

        {
            const VkViewport viewport{
                .width = f32(context.width),
                .height = f32(context.height),
                .minDepth = 0.f,
                .maxDepth = 1.f,
            };

            const VkRect2D scissor{.extent{.width = context.width, .height = context.height}};

            vkCmdSetViewport(commandBuffer.get(), 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer.get(), 0, 1, &scissor);
        }

        vkCmdBeginRendering(commandBuffer.get(), &renderInfo);

        const VkBuffer vertexBuffers[] = {m_positions.buffer, m_colors.buffer};
        constexpr VkDeviceSize offsets[] = {0, 0};
        vkCmdBindVertexBuffers(commandBuffer.get(), 0, 2, vertexBuffers, offsets);

        vkCmdDraw(commandBuffer.get(), 3, 1, 0, 0);

        vkCmdEndRendering(commandBuffer.get());
    }

    bool hello_world::create_shader_modules(frame_allocator& allocator, VkDevice device)
    {
        const shader_compiler::scope compiler{};

        m_vertShaderModule =
            shader_compiler::create_shader_module_from_glsl_file(allocator,
                                                                 device,
                                                                 VK_SHADER_STAGE_VERTEX_BIT,
                                                                 "./shaders/hello_world/hello_world.vert");

        m_fragShaderModule =
            shader_compiler::create_shader_module_from_glsl_file(allocator,
                                                                 device,
                                                                 VK_SHADER_STAGE_FRAGMENT_BIT,
                                                                 "./shaders/hello_world/hello_world.frag");

        return m_vertShaderModule && m_fragShaderModule;
    }

    bool hello_world::create_graphics_pipeline(VkDevice device, const VkFormat swapchainFormat)
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        {
            return false;
        }

        const VkPipelineShaderStageCreateInfo vertShaderStageInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = m_vertShaderModule,
            .pName = "main"};

        const VkPipelineShaderStageCreateInfo fragShaderStageInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = m_fragShaderModule,
            .pName = "main"};

        constexpr VkVertexInputBindingDescription vertexInputBindingDescs[] = {
            {.binding = 0, .stride = sizeof(vec2), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
            {.binding = 1, .stride = sizeof(vec3), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}};

        constexpr VkVertexInputAttributeDescription vertexInputAttributeDescs[] = {
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0},
            {.location = 1, .binding = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0}};

        const VkPipelineVertexInputStateCreateInfo vertexInputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = array_size(vertexInputBindingDescs),
            .pVertexBindingDescriptions = vertexInputBindingDescs,
            .vertexAttributeDescriptionCount = array_size(vertexInputAttributeDescs),
            .pVertexAttributeDescriptions = vertexInputAttributeDescs};

        const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE};

        const VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        const VkPipelineViewportStateCreateInfo viewportState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        const VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.f,
        };

        const VkPipelineMultisampleStateCreateInfo multisampling{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .minSampleShading = 1.f,
        };

        const VkPipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT,
        };

        const VkPipelineColorBlendStateCreateInfo colorBlending{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
            .blendConstants = {0.f},
        };

        const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapchainFormat,
        };

        constexpr VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        const VkPipelineDynamicStateCreateInfo dynamicState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = array_size(dynamicStates),
            .pDynamicStates = dynamicStates,
        };

        const VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &pipelineRenderingCreateInfo,
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = m_pipelineLayout,
            .renderPass = nullptr,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1,
        };

        return vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) ==
               VK_SUCCESS;
    }

    bool hello_world::create_vertex_buffers(allocator& allocator)
    {
        constexpr vec2 positions[] = {{0.0f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
        constexpr vec3 colors[] = {{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

        if (allocator.create_buffer({.size = sizeof(positions),
                                     .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     .memoryUsage = memory_usage::cpu_to_gpu},
                                    &m_positions) != VK_SUCCESS ||
            allocator.create_buffer({.size = sizeof(colors),
                                     .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     .memoryUsage = memory_usage::cpu_to_gpu},
                                    &m_colors) != VK_SUCCESS)
        {
            return false;
        }

        if (void* data; allocator.map(m_positions.allocation, &data) == VK_SUCCESS)
        {
            std::memcpy(data, positions, sizeof(positions));
            allocator.unmap(m_positions.allocation);
        }

        if (void* data; allocator.map(m_colors.allocation, &data) == VK_SUCCESS)
        {
            std::memcpy(data, colors, sizeof(colors));
            allocator.unmap(m_colors.allocation);
        }

        return true;
    }

    void hello_world::destroy_graphics_pipeline(VkDevice device)
    {
        if (m_pipelineLayout)
        {
            vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
            m_pipelineLayout = nullptr;
        }

        if (m_graphicsPipeline)
        {
            vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
            m_graphicsPipeline = nullptr;
        }
    }

    void hello_world::update_imgui(const sandbox_update_imgui_context&)
    {
        ImGui::ShowDemoWindow();
    }
}