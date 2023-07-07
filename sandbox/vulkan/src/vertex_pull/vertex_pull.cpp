#include <vertex_pull/vertex_pull.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/math/angle.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/shader_compiler.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <sandbox/context.hpp>

#include <imgui.h>

namespace oblo::vk
{
    namespace
    {
        constexpr u32 MaxBatchesCount{64u};
    }

    VkPhysicalDeviceFeatures vertex_pull::get_required_physical_device_features() const
    {
        return {.multiDrawIndirect = VK_TRUE, .shaderInt64 = VK_TRUE};
    }

    void* vertex_pull::get_device_features_list() const
    {
        static VkPhysicalDeviceHostQueryResetFeatures queryReset{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES,
            .hostQueryReset = VK_TRUE,
        };

        static VkPhysicalDeviceBufferDeviceAddressFeatures deviceAddress{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = &queryReset,
            .bufferDeviceAddress = VK_TRUE,
        };

        static VkPhysicalDeviceShaderDrawParametersFeatures drawParameters{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
            .pNext = &deviceAddress,
            .shaderDrawParameters = VK_TRUE,
        };

        return &drawParameters;
    };

    bool vertex_pull::init(const sandbox_init_context& context)
    {
        create_geometry();
        compute_layout_params();

        const VkDevice device = context.engine->get_device();

        return compile_shader_modules(device) && create_pools(device) && create_descriptor_set_layouts(device) &&
               create_pipelines(device, context.swapchainFormat) && create_buffers(device, *context.allocator);
    }

    void vertex_pull::shutdown(const sandbox_shutdown_context& context)
    {
        const VkDevice device = context.engine->get_device();
        destroy_buffers(*context.allocator);
        destroy_pipelines(device);
        destroy_pools(device);
        destroy_shader_modules(device);
    }

    void vertex_pull::update(const sandbox_render_context& context)
    {
        const VkCommandBuffer commandBuffer = context.commandBuffer->get();

        {
            const auto swapchainTexture = context.resourceManager->get(context.swapchainTexture);

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

        VkPipeline pipeline{nullptr};

        switch (m_method)
        {
        case method::vertex_buffers:
        case method::vertex_buffers_indirect:
            pipeline = m_vertexBuffersPipeline;
            break;
        case method::vertex_pull_indirect:
            pipeline = m_vertexPullPipeline;
            break;
        case method::vertex_pull_merge:
            pipeline = m_vertexPullMergePipeline;
            break;
        }

        const auto& swapchainTexture = context.resourceManager->get(context.swapchainTexture);

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

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

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

        const auto poolIndex = context.frameIndex % MaxFramesInFlight;
        const VkDevice device = context.engine->get_device();
        const VkDescriptorPool descriptorPool = m_descriptorPools[poolIndex];
        vkResetDescriptorPool(device, descriptorPool, 0);

        const VkQueryPool queryPool = m_queryPools[poolIndex];

        u64 timestamps[2 * MaxBatchesCount];

        u32 expectedTimestamps{0};

        switch (m_method)
        {
        case method::vertex_buffers_indirect:
        case method::vertex_pull_indirect:
            expectedTimestamps = 2 * m_batchesCount;
            break;

        case method::vertex_pull_merge:
            expectedTimestamps = 2;
            break;
        default:
            break;
        }

        if (expectedTimestamps > 0 && expectedTimestamps == m_enqueuedTimestamps[poolIndex] &&
            vkGetQueryPoolResults(device,
                                  queryPool,
                                  0,
                                  expectedTimestamps,
                                  sizeof(timestamps),
                                  timestamps,
                                  sizeof(u64),
                                  VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) == VK_SUCCESS)
        {
            m_lastRecordedTime = 0;

            for (u32 i = 0; i < expectedTimestamps; i += 2)
            {
                m_lastRecordedTime += timestamps[i + 1] - timestamps[i];
            }
        }

        m_enqueuedTimestamps[poolIndex] = 0;
        vkResetQueryPool(device, queryPool, 0, 2 * MaxBatchesCount);

        vkCmdBeginRendering(commandBuffer, &renderInfo);

        switch (m_method)
        {
        case method::vertex_buffers: {
            for (u32 batchIndex = 0; batchIndex < m_batchesCount; ++batchIndex)
            {
                const VkBuffer vertexBuffers[] = {m_positionBuffers[batchIndex].buffer,
                                                  m_colorBuffers[batchIndex].buffer};
                constexpr VkDeviceSize offsets[] = {0, 0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

                for (u32 objectIndex = 0; objectIndex < m_objectsPerBatch; ++objectIndex)
                {
                    vkCmdDraw(commandBuffer, m_verticesPerObject, 1, m_verticesPerObject * objectIndex, 0);
                }
            }
            break;
        }
        case method::vertex_buffers_indirect: {
            for (u32 batchIndex = 0; batchIndex < m_batchesCount; ++batchIndex)
            {
                const VkBuffer vertexBuffers[] = {m_positionBuffers[batchIndex].buffer,
                                                  m_colorBuffers[batchIndex].buffer};
                constexpr VkDeviceSize offsets[] = {0, 0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, queryPool, 2 * batchIndex);

                vkCmdDrawIndirect(commandBuffer,
                                  m_indirectDrawBuffers[batchIndex].buffer,
                                  0,
                                  m_objectsPerBatch,
                                  sizeof(VkDrawIndirectCommand));

                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, queryPool, 2 * batchIndex + 1);
            }

            m_enqueuedTimestamps[poolIndex] = u8(2 * m_batchesCount);
        }
        break;
        case method::vertex_pull_indirect: {
            for (u32 batchIndex = 0; batchIndex < m_batchesCount; ++batchIndex)
            {
                const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = descriptorPool,
                    .descriptorSetCount = 1,
                    .pSetLayouts = &m_vertexPullSetLayout,
                };

                VkDescriptorSet descriptorSet;
                OBLO_VK_PANIC(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

                const VkDescriptorBufferInfo positionsBuffer{
                    m_positionBuffers[batchIndex].buffer,
                    0,
                    m_verticesPerObject * m_objectsPerBatch * sizeof(vec3),
                };

                const VkDescriptorBufferInfo colorsBuffer{
                    m_colorBuffers[batchIndex].buffer,
                    0,
                    m_verticesPerObject * m_objectsPerBatch * sizeof(vec3),
                };

                const VkWriteDescriptorSet descriptorSetWrites[]{
                    {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = descriptorSet,
                        .dstBinding = 0,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .pBufferInfo = &positionsBuffer,
                    },
                    {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = descriptorSet,
                        .dstBinding = 1,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .pBufferInfo = &colorsBuffer,
                    },
                };

                vkUpdateDescriptorSets(device, array_size(descriptorSetWrites), descriptorSetWrites, 0, nullptr);

                vkCmdBindDescriptorSets(commandBuffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        m_vertexPullPipelineLayout,
                                        0,
                                        1,
                                        &descriptorSet,
                                        0,
                                        nullptr);

                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, queryPool, 2 * batchIndex);

                vkCmdDrawIndirect(commandBuffer,
                                  m_indirectDrawBuffers[batchIndex].buffer,
                                  0,
                                  m_objectsPerBatch,
                                  sizeof(VkDrawIndirectCommand));

                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, queryPool, 2 * batchIndex + 1);
            }

            m_enqueuedTimestamps[poolIndex] = u8(2 * m_batchesCount);
        }
        break;
        case method::vertex_pull_merge: {
            const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &m_vertexPullMergeSetLayout,
            };

            VkDescriptorSet descriptorSet;
            OBLO_VK_PANIC(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

            const VkDescriptorBufferInfo positionsBuffer{
                m_positionBuffersRefs.buffer,
                0,
                m_batchesCount * sizeof(u64),
            };

            const VkDescriptorBufferInfo colorsBuffer{
                m_colorBuffersRefs.buffer,
                0,
                m_batchesCount * sizeof(u64),
            };

            const VkDescriptorBufferInfo mergeBuffer{
                m_mergeIndirectionBuffer.buffer,
                0,
                m_batchesCount * m_objectsPerBatch * sizeof(u32),
            };

            const VkWriteDescriptorSet descriptorSetWrites[]{
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSet,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &positionsBuffer,
                },
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSet,
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &colorsBuffer,
                },
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSet,
                    .dstBinding = 2,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &mergeBuffer,
                },
            };

            vkUpdateDescriptorSets(device, array_size(descriptorSetWrites), descriptorSetWrites, 0, nullptr);

            vkCmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_vertexPullMergePipelineLayout,
                                    0,
                                    1,
                                    &descriptorSet,
                                    0,
                                    nullptr);

            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, queryPool, 0);

            vkCmdDrawIndirect(commandBuffer,
                              m_mergeIndirectDrawCommandsBuffer.buffer,
                              0,
                              m_objectsPerBatch * m_batchesCount,
                              sizeof(VkDrawIndirectCommand));

            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, queryPool, 1);

            m_enqueuedTimestamps[poolIndex] = 2;
        }
        break;
        default:
            break;
        }

        vkCmdEndRendering(commandBuffer);
    }

    void vertex_pull::update_imgui(const sandbox_update_imgui_context& context)
    {
        if (ImGui::Begin("Configuration", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            bool updateGeometry{false};

            constexpr const char* items[] = {
                "VB - Draw per Batch",
                "VB - Indirect Draw per Batch",
                "SSBO - Indirect Draw per Batch",
                "SSBO - Indirect Draw Merge Batches",
            };

            if (ImGui::BeginCombo("Draw Method", items[u32(m_method)]))
            {
                for (u32 i = 0; i < array_size(items); ++i)
                {
                    const bool isSelected = u32(m_method) == i;

                    if (ImGui::Selectable(items[i], isSelected))
                    {
                        const auto selectedMethod = method(i);

                        if (selectedMethod != m_method)
                        {
                            m_method = selectedMethod;
                            m_lastRecordedTime = 0;
                            for (auto& enqueuedTimestamp : m_enqueuedTimestamps)
                            {
                                enqueuedTimestamp = 0;
                            }
                        }
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }

            {
                u32 min{1};
                u32 max{MaxBatchesCount};

                if (ImGui::SliderScalar("Batches Count", ImGuiDataType_U32, &m_batchesCount, &min, &max))
                {
                    updateGeometry = true;
                }
            }

            {
                u32 min{1};
                u32 max{1 << 17};

                if (ImGui::SliderScalar("Objects per Batch", ImGuiDataType_U32, &m_objectsPerBatch, &min, &max))
                {
                    updateGeometry = true;
                }
            }

            if (m_lastRecordedTime != 0)
            {
                using output_time_type = std::chrono::duration<float, std::milli>;
                const auto nsTime = std::chrono::nanoseconds{m_lastRecordedTime};
                const auto msFloatTime = std::chrono::duration_cast<output_time_type>(nsTime).count();

                switch (m_method)
                {
                case method::vertex_buffers_indirect:
                case method::vertex_pull_indirect:
                case method::vertex_pull_merge:
                    ImGui::Text("Vertex Shader Time Total (ms): %f", msFloatTime);
                    ImGui::Text("Vertex Shader Time Average Per Batch (ms): %f", msFloatTime / m_batchesCount);
                    break;
                default:
                    break;
                }
            }

            if (updateGeometry)
            {
                const auto device = context.engine->get_device();
                vkDeviceWaitIdle(device);

                destroy_buffers(*context.allocator);

                create_geometry();
                compute_layout_params();

                create_buffers(device, *context.allocator);
            }
        }
    }

    bool vertex_pull::compile_shader_modules(VkDevice device)
    {
        shader_compiler compiler;

        m_shaderVertexBuffersVert =
            compiler.create_shader_module_from_glsl_file(device,
                                                         "./shaders/vertex_pull/vertex_buffers.vert",
                                                         VK_SHADER_STAGE_VERTEX_BIT);

        m_shaderVertexPullVert = compiler.create_shader_module_from_glsl_file(device,
                                                                              "./shaders/vertex_pull/vertex_pull.vert",
                                                                              VK_SHADER_STAGE_VERTEX_BIT);

        m_shaderVertexPullMergeVert =
            compiler.create_shader_module_from_glsl_file(device,
                                                         "./shaders/vertex_pull/vertex_pull_merge.vert",
                                                         VK_SHADER_STAGE_VERTEX_BIT);

        m_shaderSharedFrag = compiler.create_shader_module_from_glsl_file(device,
                                                                          "./shaders/vertex_pull/shared.frag",
                                                                          VK_SHADER_STAGE_FRAGMENT_BIT);

        return m_shaderVertexBuffersVert && m_shaderSharedFrag && m_shaderVertexPullMergeVert;
    }

    bool vertex_pull::create_pools(VkDevice device)
    {
        constexpr VkDescriptorPoolSize descriptorSizes[]{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MaxBatchesCount * 2}};

        const VkDescriptorPoolCreateInfo poolCreateInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                        .maxSets = MaxBatchesCount,
                                                        .poolSizeCount = array_size(descriptorSizes),
                                                        .pPoolSizes = descriptorSizes};

        for (auto& descriptorPool : m_descriptorPools)
        {
            if (vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS)
            {
                return false;
            }
        }

        const VkQueryPoolCreateInfo queryPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = 2 * MaxBatchesCount,
        };

        for (auto& queryPool : m_queryPools)
        {
            if (vkCreateQueryPool(device, &queryPoolCreateInfo, nullptr, &queryPool) != VK_SUCCESS)
            {
                return false;
            }
        }

        return true;
    }

    bool vertex_pull::create_descriptor_set_layouts(VkDevice device)
    {
        constexpr VkDescriptorSetLayoutBinding pullBufferBindings[]{
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
        };

        const VkDescriptorSetLayoutCreateInfo pullSetLayoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = array_size(pullBufferBindings),
            .pBindings = pullBufferBindings,
        };

        constexpr VkDescriptorSetLayoutBinding pullMergeBufferBindings[]{
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
            {
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
        };

        const VkDescriptorSetLayoutCreateInfo pullMergeSetLayoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = array_size(pullMergeBufferBindings),
            .pBindings = pullMergeBufferBindings,
        };

        return vkCreateDescriptorSetLayout(device, &pullSetLayoutCreateInfo, nullptr, &m_vertexPullSetLayout) ==
                   VK_SUCCESS &&
               vkCreateDescriptorSetLayout(device,
                                           &pullMergeSetLayoutCreateInfo,
                                           nullptr,
                                           &m_vertexPullMergeSetLayout) == VK_SUCCESS;
    }

    bool vertex_pull::create_buffers(VkDevice device, allocator& allocator)
    {
        const auto totalVerticesCount = m_objectsPerBatch * m_verticesPerObject;
        const auto positionsSize = u32(totalVerticesCount * sizeof(m_positions[0]));
        const auto colorsSize = u32(totalVerticesCount * sizeof(m_colors[0]));
        const auto indirectDrawSize = u32(m_indirectDrawCommands.size() * sizeof(m_indirectDrawCommands[0]));

        constexpr auto hex_rgb = [](u32 rgb)
        {
            return vec3{
                .x = ((rgb & 0xFF0000) >> 16) / 255.f,
                .y = ((rgb & 0xFF00) >> 8) / 255.f,
                .z = (rgb & 0xFF) / 255.f,
            };
        };

        constexpr vec3 colors[] = {
            hex_rgb(0x21C0C0),
            hex_rgb(0xC1E996),
            hex_rgb(0xF5C16C),
            hex_rgb(0xEE4349),
            hex_rgb(0x203551),
            hex_rgb(0x243B31),
            hex_rgb(0x581123),
            hex_rgb(0xF0DFB6),
        };

        m_positionBuffers.assign(m_batchesCount, {});
        m_colorBuffers.assign(m_batchesCount, {});
        m_indirectDrawBuffers.assign(m_batchesCount, {});

        std::vector<vec3> transformedPositions;
        transformedPositions.resize(m_positions.size());

        for (u32 batchIndex = 0; batchIndex < m_batchesCount; ++batchIndex)
        {
            for (u32 vertexIndex = 0; vertexIndex < m_positions.size(); ++vertexIndex)
            {
                const auto translation =
                    vec3{m_layoutOffset + 2.f * m_layoutQuadScale * f32(batchIndex % m_layoutQuadsPerRow),
                         m_layoutOffset + 2.f * m_layoutQuadScale * f32(batchIndex / m_layoutQuadsPerRow),
                         0.f};

                transformedPositions[vertexIndex] = m_positions[vertexIndex] * m_layoutQuadScale + translation;
            }

            m_colors.assign(totalVerticesCount, colors[batchIndex % array_size(colors)]);

            auto& positionBuffer = m_positionBuffers[batchIndex];
            auto& colorBuffer = m_colorBuffers[batchIndex];
            auto& indirectDrawBuffer = m_indirectDrawBuffers[batchIndex];

            // Different modes need different flags, not sure if it's a problem to use them all together
            constexpr auto vertexBufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

            if (allocator.create_buffer(
                    {.size = positionsSize, .usage = vertexBufferUsage, .memoryUsage = memory_usage::cpu_to_gpu},
                    &positionBuffer) != VK_SUCCESS ||
                allocator.create_buffer(
                    {.size = colorsSize, .usage = vertexBufferUsage, .memoryUsage = memory_usage::cpu_to_gpu},
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
                std::memcpy(data, transformedPositions.data(), positionsSize);
                allocator.unmap(positionBuffer.allocation);
            }

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

        const auto mergeIndirectionSize = u32(m_mergeIndirection.size() * sizeof(m_mergeIndirection[0]));
        const auto refBufferSize = u32(sizeof(u64) * MaxBatchesCount);

        if (allocator.create_buffer({.size = mergeIndirectionSize,
                                     .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     .memoryUsage = memory_usage::cpu_to_gpu},
                                    &m_mergeIndirectionBuffer) != VK_SUCCESS ||
            allocator.create_buffer({.size = indirectDrawSize * m_batchesCount,
                                     .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                     .memoryUsage = memory_usage::cpu_to_gpu},
                                    &m_mergeIndirectDrawCommandsBuffer) != VK_SUCCESS ||
            allocator.create_buffer({.size = refBufferSize,
                                     .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     .memoryUsage = memory_usage::cpu_to_gpu},
                                    &m_positionBuffersRefs) != VK_SUCCESS ||
            allocator.create_buffer({.size = refBufferSize,
                                     .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     .memoryUsage = memory_usage::cpu_to_gpu},
                                    &m_colorBuffersRefs) != VK_SUCCESS)
        {
            return false;
        }

        if (void* data; allocator.map(m_mergeIndirectionBuffer.allocation, &data) == VK_SUCCESS)
        {
            std::memcpy(data, m_mergeIndirection.data(), mergeIndirectionSize);
            allocator.unmap(m_mergeIndirectionBuffer.allocation);
        }

        if (void* data; allocator.map(m_mergeIndirectDrawCommandsBuffer.allocation, &data) == VK_SUCCESS)
        {
            for (u32 i = 0; i < m_batchesCount; ++i)
            {
                std::memcpy(data, m_indirectDrawCommands.data(), indirectDrawSize);
                data = reinterpret_cast<u8*>(data) + indirectDrawSize;
            }

            allocator.unmap(m_mergeIndirectDrawCommandsBuffer.allocation);
        }

        if (void* data; allocator.map(m_positionBuffersRefs.allocation, &data) == VK_SUCCESS)
        {
            const auto begin = reinterpret_cast<u64*>(data);
            const auto end = begin + m_batchesCount;

            for (u64 *outDeviceAddress = begin, batchIndex = 0; outDeviceAddress != end;
                 ++outDeviceAddress, ++batchIndex)
            {
                const VkBufferDeviceAddressInfo deviceAddressInfo{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                    .buffer = m_positionBuffers[batchIndex].buffer,
                };

                const u64 deviceAddress = vkGetBufferDeviceAddress(device, &deviceAddressInfo);
                std::memcpy(outDeviceAddress, &deviceAddress, sizeof(u64));
            }
        }

        if (void* data; allocator.map(m_colorBuffersRefs.allocation, &data) == VK_SUCCESS)
        {
            const auto begin = reinterpret_cast<u64*>(data);
            const auto end = begin + m_batchesCount;

            for (u64 *outDeviceAddress = begin, batchIndex = 0; outDeviceAddress != end;
                 ++outDeviceAddress, ++batchIndex)
            {
                const VkBufferDeviceAddressInfo deviceAddressInfo{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                    .buffer = m_colorBuffers[batchIndex].buffer,
                };

                const u64 deviceAddress = vkGetBufferDeviceAddress(device, &deviceAddressInfo);
                std::memcpy(outDeviceAddress, &deviceAddress, sizeof(u64));
            }
        }

        return true;
    }

    bool vertex_pull::create_pipelines(VkDevice device, VkFormat swapchainFormat)
    {
        const VkPipelineLayoutCreateInfo vertexBuffersPipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        };

        const VkPipelineLayoutCreateInfo vertexPullPipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &m_vertexPullSetLayout,
        };

        const VkPipelineLayoutCreateInfo vertexPullMergePipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &m_vertexPullMergeSetLayout,
        };

        if (vkCreatePipelineLayout(device, &vertexBuffersPipelineLayoutInfo, nullptr, &m_vertexBuffersPipelineLayout) !=
                VK_SUCCESS ||
            vkCreatePipelineLayout(device, &vertexPullPipelineLayoutInfo, nullptr, &m_vertexPullPipelineLayout) !=
                VK_SUCCESS ||
            vkCreatePipelineLayout(device,
                                   &vertexPullMergePipelineLayoutInfo,
                                   nullptr,
                                   &m_vertexPullMergePipelineLayout) != VK_SUCCESS)
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

        const VkPipelineShaderStageCreateInfo vertexBufferStages[] = {vertexBufferVertexShaderInfo,
                                                                      vertexBufferFragmentShaderInfo};

        const VkPipelineShaderStageCreateInfo vertexPullVertexShaderInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = m_shaderVertexPullVert,
            .pName = "main"};

        const VkPipelineShaderStageCreateInfo vertexPullStages[] = {vertexPullVertexShaderInfo,
                                                                    vertexBufferFragmentShaderInfo};

        const VkPipelineShaderStageCreateInfo vertexPullMergeVertexShaderInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = m_shaderVertexPullMergeVert,
            .pName = "main"};

        const VkPipelineShaderStageCreateInfo vertexPullMergeStages[] = {vertexPullMergeVertexShaderInfo,
                                                                         vertexBufferFragmentShaderInfo};

        const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        constexpr VkVertexInputBindingDescription vertexInputBindingDescs[]{
            {.binding = 0, .stride = sizeof(vec3), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
            {.binding = 1, .stride = sizeof(vec3), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
        };

        constexpr VkVertexInputAttributeDescription vertexInputAttributeDescs[]{
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
            {.location = 1, .binding = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
        };

        const VkPipelineVertexInputStateCreateInfo vertexBufferInputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = array_size(vertexInputBindingDescs),
            .pVertexBindingDescriptions = vertexInputBindingDescs,
            .vertexAttributeDescriptionCount = array_size(vertexInputAttributeDescs),
            .pVertexAttributeDescriptions = vertexInputAttributeDescs,
        };

        const VkPipelineVertexInputStateCreateInfo vertexPullInputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        };

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

        const VkGraphicsPipelineCreateInfo pipelineInfos[]{
            {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = &pipelineRenderingCreateInfo,
                .stageCount = 2,
                .pStages = vertexBufferStages,
                .pVertexInputState = &vertexBufferInputInfo,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = nullptr,
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicState,
                .layout = m_vertexBuffersPipelineLayout,
                .renderPass = nullptr,
                .subpass = 0,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1,
            },
            {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = &pipelineRenderingCreateInfo,
                .stageCount = 2,
                .pStages = vertexPullStages,
                .pVertexInputState = &vertexPullInputInfo,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = nullptr,
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicState,
                .layout = m_vertexPullPipelineLayout,
                .renderPass = nullptr,
                .subpass = 0,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1,
            },
            {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = &pipelineRenderingCreateInfo,
                .stageCount = 2,
                .pStages = vertexPullMergeStages,
                .pVertexInputState = &vertexPullInputInfo,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = nullptr,
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicState,
                .layout = m_vertexPullMergePipelineLayout,
                .renderPass = nullptr,
                .subpass = 0,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1,
            },
        };

        VkPipeline pipelines[3];

        bool success = vkCreateGraphicsPipelines(device,
                                                 VK_NULL_HANDLE,
                                                 array_size(pipelineInfos),
                                                 pipelineInfos,
                                                 nullptr,
                                                 pipelines) == VK_SUCCESS;

        m_vertexBuffersPipeline = pipelines[0];
        m_vertexPullPipeline = pipelines[1];
        m_vertexPullMergePipeline = pipelines[2];

        return success;
    }

    void vertex_pull::create_geometry()
    {
        constexpr vec3 positions[] = {{0.0f, -0.5f, 0.f}, {-0.5f, 0.5f, 0.f}, {0.5f, 0.5f, 0.f}};
        m_verticesPerObject = array_size(positions);

        const auto totalVerticesCount = m_objectsPerBatch * m_verticesPerObject;

        m_positions.clear();
        m_positions.reserve(totalVerticesCount);

        radians rotation{};
        const radians delta{360_deg / f32(m_objectsPerBatch)};

        for (u32 i = 0; i < m_objectsPerBatch; ++i, rotation = rotation + delta)
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

        m_colors.assign(totalVerticesCount, vec3{});

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

        m_mergeIndirection.clear();
        m_mergeIndirection.reserve(m_objectsPerBatch * m_batchesCount);

        for (u32 batchIndex = 0; batchIndex < m_batchesCount; ++batchIndex)
        {
            m_mergeIndirection.insert(m_mergeIndirection.end(), m_objectsPerBatch, batchIndex);
        }
    }

    void vertex_pull::destroy_buffers(allocator& allocator)
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

        allocator.destroy(m_mergeIndirectionBuffer);
        m_mergeIndirectionBuffer = {};

        allocator.destroy(m_mergeIndirectDrawCommandsBuffer);
        m_mergeIndirectDrawCommandsBuffer = {};

        allocator.destroy(m_positionBuffersRefs);
        m_positionBuffersRefs = {};

        allocator.destroy(m_colorBuffersRefs);
        m_colorBuffersRefs = {};
    }

    void vertex_pull::destroy_pipelines(VkDevice device)
    {
        reset_device_objects(device,
                             m_vertexBuffersPipelineLayout,
                             m_vertexPullPipelineLayout,
                             m_vertexPullMergePipelineLayout,
                             m_vertexBuffersPipeline,
                             m_vertexPullPipeline,
                             m_vertexPullMergePipeline,
                             m_vertexPullSetLayout,
                             m_vertexPullMergeSetLayout);
    }

    void vertex_pull::destroy_shader_modules(VkDevice device)
    {
        reset_device_objects(device,
                             m_shaderVertexBuffersVert,
                             m_shaderVertexPullVert,
                             m_shaderVertexPullMergeVert,
                             m_shaderSharedFrag);
    }

    void vertex_pull::destroy_pools(VkDevice device)
    {
        reset_device_objects(device, std::span{m_descriptorPools}, std::span{m_queryPools});
    }

    void vertex_pull::compute_layout_params()
    {
        m_layoutQuadsPerRow = u32(std::ceilf(std::sqrtf(f32(m_batchesCount))));
        m_layoutQuadScale = {1.f / m_layoutQuadsPerRow};
        m_layoutOffset = m_layoutQuadScale - 1.f;
    }
}