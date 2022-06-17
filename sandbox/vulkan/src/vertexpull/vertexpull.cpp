#include <vertexpull/vertexpull.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/shader_compiler.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <sandbox/context.hpp>

namespace oblo::vk
{

    bool vertexpull::init(const sandbox_init_context& context)
    {
        create_geometry();

        const VkDevice device = context.engine->get_device();
        return compile_shader_modules(device) && create_pipelines(device, context.swapchainFormat) &&
               create_vertex_buffers(*context.allocator, m_objectsPerBatch);
    }

    void vertexpull::shutdown(const sandbox_shutdown_context& context)
    {
        const VkDevice device = context.engine->get_device();
        destroy_buffers(*context.allocator);
        destroy_pipelines(device);
        destroy_shader_modules(device);
    }

    void vertexpull::update(const sandbox_render_context& context)
    {
        const VkCommandBuffer commandBuffer = context.commandBuffer;

        {
            const VkImageMemoryBarrier imageMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .image = context.swapchainImage,
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
            };

            vkCmdPipelineBarrier(commandBuffer,
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

        const VkRenderingAttachmentInfo colorAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = context.swapchainImageView,
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

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vertexBuffersPipeline);

        {
            const u32 width = context.width;
            const u32 height = context.height;

            const VkViewport viewport{
                .width = f32(width),
                .height = f32(height),
                .minDepth = 0.f,
                .maxDepth = 1.f,
            };

            const VkRect2D scissor{.extent{.width = width, .height = height}};

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        }

        vkCmdBeginRendering(commandBuffer, &renderInfo);

        for (u32 batchIndex = 0; batchIndex < BatchesCount; ++batchIndex)
        {
            const VkBuffer vertexBuffers[] = {m_positionBuffers[batchIndex].buffer, m_colorBuffers[batchIndex].buffer};
            constexpr VkDeviceSize offsets[] = {0, 0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

            vkCmdDraw(commandBuffer, m_positions.size(), 1, 0, 0);
        }

        vkCmdEndRendering(commandBuffer);
    }

    void vertexpull::update_imgui() {}

    bool vertexpull::compile_shader_modules(VkDevice device)
    {
        shader_compiler compiler;

        m_shaderVertexBuffersVert =
            compiler.create_shader_module_from_glsl_file(device,
                                                         "./shaders/vertexpull/vertex_buffers.vert",
                                                         VK_SHADER_STAGE_VERTEX_BIT);

        m_shaderSharedFrag = compiler.create_shader_module_from_glsl_file(device,
                                                                          "./shaders/vertexpull/shared.frag",
                                                                          VK_SHADER_STAGE_FRAGMENT_BIT);

        return m_shaderVertexBuffersVert && m_shaderSharedFrag;
    }

    bool vertexpull::create_pipelines(VkDevice device, VkFormat swapchainFormat)
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        {
            return false;
        }

        const VkPipelineShaderStageCreateInfo vertexBufferVertexShaderInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = m_shaderVertexBuffersVert,
            .pName = "main"};

        const VkPipelineShaderStageCreateInfo vertexBufferFragmentShaderInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = m_shaderSharedFrag,
            .pName = "main"};

        const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        const VkPipelineShaderStageCreateInfo vertexBufferStages[] = {vertexBufferVertexShaderInfo,
                                                                      vertexBufferFragmentShaderInfo};

        constexpr VkVertexInputBindingDescription vertexInputBindingDescs[]{
            {.binding = 0, .stride = sizeof(vec3), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
            {.binding = 1, .stride = sizeof(vec3), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
        };

        constexpr VkVertexInputAttributeDescription vertexInputAttributeDescs[]{
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
            {.location = 1, .binding = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
        };

        const VkPipelineVertexInputStateCreateInfo vertexInputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = array_size(vertexInputBindingDescs),
            .pVertexBindingDescriptions = vertexInputBindingDescs,
            .vertexAttributeDescriptionCount = array_size(vertexInputAttributeDescs),
            .pVertexAttributeDescriptions = vertexInputAttributeDescs};

        const VkPipelineViewportStateCreateInfo viewportState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        const VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
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
            .pStages = vertexBufferStages,
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

        return vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_vertexBuffersPipeline) ==
               VK_SUCCESS;
    }

    bool vertexpull::create_vertex_buffers(allocator& allocator, u32)
    {
        const auto positionsSize = u32(m_positions.size() * sizeof(m_positions[0]));
        const auto colorsSize = u32(m_colors.size() * sizeof(m_colors[0]));

        constexpr auto hex = [](u8 r, u8 g, u8 b) { return vec3{r / 255.f, g / 255.f, b / 255.f}; };

        constexpr vec3 colors[BatchesCount] = {
            hex(0x21, 0xC0, 0xC0),
            hex(0xC1, 0xE9, 0x96),
            hex(0xF5, 0xC1, 0x6C),
            hex(0xEE, 0x43, 0x49),
            hex(0x20, 0x35, 0x51),
            hex(0x24, 0x3B, 0x31),
            hex(0x58, 0x11, 0x23),
            hex(0xF0, 0xDF, 0xB6),
        };

        for (u32 i = 0; i < BatchesCount; ++i)
        {
            auto& positionBuffer = m_positionBuffers[i];
            auto& colorBuffer = m_colorBuffers[i];

            if (allocator.create_buffer({.size = positionsSize,
                                         .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         .memoryUsage = memory_usage::cpu_to_gpu},
                                        &positionBuffer) != VK_SUCCESS ||
                allocator.create_buffer({.size = colorsSize,
                                         .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         .memoryUsage = memory_usage::cpu_to_gpu},
                                        &colorBuffer) != VK_SUCCESS)
            {
                return false;
            }

            if (void* data; allocator.map(positionBuffer.allocation, &data) == VK_SUCCESS)
            {
                std::memcpy(data, m_positions.data(), positionsSize);
                allocator.unmap(positionBuffer.allocation);
            }

            m_colors.assign(m_colors.size(), colors[i]);

            if (void* data; allocator.map(colorBuffer.allocation, &data) == VK_SUCCESS)
            {
                std::memcpy(data, m_colors.data(), colorsSize);
                allocator.unmap(colorBuffer.allocation);
            }
        }

        return true;
    }

    void vertexpull::create_geometry()
    {
        m_positions = {{0.0f, -0.5f, 0.f}, {-0.5f, 0.5f, 0.f}, {0.5f, 0.5f, 0.f}};
        m_colors.resize(m_positions.size());
    }

    void vertexpull::destroy_buffers(allocator& allocator)
    {
        for (auto& buffer : m_positionBuffers)
        {
            allocator.destroy(buffer);
            buffer = {};
        }

        for (auto& buffer : m_colorBuffers)
        {
            allocator.destroy(buffer);
            buffer = {};
        }
    }

    void vertexpull::destroy_pipelines(VkDevice device)
    {
        reset_device_objects(device, m_pipelineLayout, m_vertexBuffersPipeline);
    }

    void vertexpull::destroy_shader_modules(VkDevice device)
    {
        reset_device_objects(device, m_shaderVertexBuffersVert, m_shaderSharedFrag);
    }
}