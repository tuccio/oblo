#include <vertexpull/vertexpull.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/math/angle.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/shader_compiler.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <sandbox/context.hpp>

#include <imgui.h>

namespace oblo::vk
{
    VkPhysicalDeviceFeatures vertexpull::get_required_physical_device_features() const
    {
        return {.multiDrawIndirect = VK_TRUE};
    }

    bool vertexpull::init(const sandbox_init_context& context)
    {
        create_geometry(m_objectsPerBatch);

        const VkDevice device = context.engine->get_device();
        return compile_shader_modules(device) && create_pipelines(device, context.swapchainFormat) &&
               create_vertex_buffers(*context.allocator);
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

        switch (m_method)
        {
        case method::vertex_buffers:
            for (u32 batchIndex = 0; batchIndex < BatchesCount; ++batchIndex)
            {
                const VkBuffer vertexBuffers[] = {m_positionBuffers[batchIndex].buffer,
                                                  m_colorBuffers[batchIndex].buffer};
                constexpr VkDeviceSize offsets[] = {0, 0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

                for (u32 objectIndex = 0; objectIndex < m_objectsPerBatch; ++objectIndex)
                {
                    vkCmdDraw(commandBuffer, m_positions.size(), 1, m_verticesPerObject * objectIndex, 0);
                }
            }
            break;
        case method::vertex_buffers_multidraw:
            for (u32 batchIndex = 0; batchIndex < BatchesCount; ++batchIndex)
            {
                const VkBuffer vertexBuffers[] = {m_positionBuffers[batchIndex].buffer,
                                                  m_colorBuffers[batchIndex].buffer};
                constexpr VkDeviceSize offsets[] = {0, 0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

                vkCmdDrawIndirect(commandBuffer,
                                  m_indirectDrawBuffers[batchIndex].buffer,
                                  0,
                                  m_objectsPerBatch,
                                  sizeof(VkDrawIndirectCommand));
            }
            break;
        default:
            break;
        }

        vkCmdEndRendering(commandBuffer);
    }

    void vertexpull::update_imgui()
    {
        if (ImGui::Begin("Configuration", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            constexpr const char* items[] = {
                "Vertex Buffers - Draw per Batch",
                "Vertex Buffers - Multidraw per Batch",
                "Vertex Pulling - Draw per Batch",
                "Vertex Pulling - Multidraw per Batch",
                "Vertex Pulling - Multidraw Merge Batches",
            };

            if (ImGui::BeginCombo("Draw Method", items[u32(m_method)]))
            {
                for (u32 i = 0; i < array_size(items); ++i)
                {
                    const bool isSelected = u32(m_method) == i;

                    if (ImGui::Selectable(items[i], isSelected))
                    {
                        m_method = method(i);
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
        }
    }

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

    bool vertexpull::create_vertex_buffers(allocator& allocator)
    {
        const auto positionsSize = u32(m_positions.size() * sizeof(m_positions[0]));
        const auto colorsSize = u32(m_colors.size() * sizeof(m_colors[0]));

        constexpr auto hex_rgb = [](u32 rgb)
        {
            return vec3{
                .x = ((rgb & 0xFF0000) >> 16) / 255.f,
                .y = ((rgb & 0xFF00) >> 8) / 255.f,
                .z = (rgb & 0xFF) / 255.f,
            };
        };

        constexpr vec3 colors[BatchesCount] = {
            hex_rgb(0x21C0C0),
            hex_rgb(0xC1E996),
            hex_rgb(0xF5C16C),
            hex_rgb(0xEE4349),
            hex_rgb(0x203551),
            hex_rgb(0x243B31),
            hex_rgb(0x581123),
            hex_rgb(0xF0DFB6),
        };

        m_indirectDrawCommands.clear();
        m_indirectDrawCommands.reserve(m_objectsPerBatch);

        for (u32 objectIndex = 0; objectIndex < m_objectsPerBatch; ++objectIndex)
        {
            m_indirectDrawCommands.push_back({
                .vertexCount = m_verticesPerObject,
                .instanceCount = 1,
                .firstVertex = m_verticesPerObject * objectIndex,
                .firstInstance = objectIndex,
            });
        }

        const auto indirectDrawSize = u32(m_indirectDrawCommands.size() * sizeof(m_indirectDrawCommands[0]));

        for (u32 i = 0; i < BatchesCount; ++i)
        {
            auto& positionBuffer = m_positionBuffers[i];
            auto& colorBuffer = m_colorBuffers[i];
            auto& indirectDrawBuffer = m_indirectDrawBuffers[i];

            if (allocator.create_buffer({.size = positionsSize,
                                         .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         .memoryUsage = memory_usage::cpu_to_gpu},
                                        &positionBuffer) != VK_SUCCESS ||
                allocator.create_buffer({.size = colorsSize,
                                         .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         .memoryUsage = memory_usage::cpu_to_gpu},
                                        &colorBuffer) != VK_SUCCESS ||
                allocator.create_buffer({.size = indirectDrawSize,
                                         .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                         .memoryUsage = memory_usage::cpu_to_gpu},
                                        &indirectDrawBuffer) != VK_SUCCESS)

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

            if (void* data; allocator.map(indirectDrawBuffer.allocation, &data) == VK_SUCCESS)
            {
                std::memcpy(data, m_indirectDrawCommands.data(), indirectDrawSize);
                allocator.unmap(indirectDrawBuffer.allocation);
            }
        }

        return true;
    }

    void vertexpull::create_geometry(u32 objectsPerBatch)
    {
        constexpr vec3 positions[] = {{0.0f, -0.5f, 0.f}, {-0.5f, 0.5f, 0.f}, {0.5f, 0.5f, 0.f}};
        m_verticesPerObject = array_size(positions);

        m_positions.clear();
        m_positions.reserve(m_objectsPerBatch * m_verticesPerObject);

        radians rotation{};
        const radians delta{360_deg / f32(objectsPerBatch)};

        for (u32 i = 0; i < objectsPerBatch; ++i, rotation = rotation + delta)
        {
            const auto s = std::sin(f32{rotation});
            const auto c = std::cos(f32{rotation});

            for (const auto& position : positions)
            {
                const f32 x = position.x * c - position.y * s;
                const f32 y = position.x * s + position.y * c;

                m_positions.push_back(vec3{x, y, 0.f});
            }
        }

        m_colors.assign(m_positions.size(), vec3{});
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

        for (auto& buffer : m_indirectDrawBuffers)
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