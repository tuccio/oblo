#include <oblo/sandbox/imgui.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/vulkan/error.hpp>

#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_vulkan.h>

#include <SDL.h>
#include <imgui.h>

#include <cstring>

namespace oblo::vk
{
    bool imgui::init(SDL_Window* window,
                     VkInstance instance,
                     VkPhysicalDevice physicalDevice,
                     VkDevice device,
                     VkQueue queue,
                     VkCommandBuffer commandBuffer,
                     u32 swapchainImageCount,
                     bool withDocking)
    {
        if (m_context)
        {
            return false;
        }

        constexpr auto poolSize{1000};

        constexpr VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, poolSize},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, poolSize},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, poolSize},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, poolSize},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, poolSize},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, poolSize},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, poolSize},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, poolSize},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, poolSize},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, poolSize},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, poolSize},
        };

        const VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = poolSize,
            .poolSizeCount = array_size(poolSizes),
            .pPoolSizes = poolSizes,
        };

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
        {
            return false;
        }

        m_context = ImGui::CreateContext();

        auto& io = ImGui::GetIO();

        io.IniFilename = nullptr;

        if (withDocking)
        {
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        }

        ImGui_ImplSDL2_InitForVulkan(window);

        ImGui_ImplVulkan_InitInfo initInfo{.Instance = instance,
                                           .PhysicalDevice = physicalDevice,
                                           .Device = device,
                                           .Queue = queue,
                                           .DescriptorPool = m_descriptorPool,
                                           .MinImageCount = swapchainImageCount,
                                           .ImageCount = swapchainImageCount,
                                           .MSAASamples = VK_SAMPLE_COUNT_1_BIT};

        if (ImGui_ImplVulkan_Init(&initInfo, nullptr) && create_dummy_pipeline(device))
        {
            const VkCommandBufferBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };

            OBLO_VK_PANIC(vkBeginCommandBuffer(commandBuffer, &beginInfo));

            bool success = ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);

            const VkSubmitInfo submitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer,
            };

            OBLO_VK_PANIC(vkEndCommandBuffer(commandBuffer));
            OBLO_VK_PANIC(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
            OBLO_VK_PANIC(vkDeviceWaitIdle(device));

            ImGui_ImplVulkan_DestroyFontUploadObjects();
            return success;
        }

        return false;
    }

    void imgui::shutdown(VkDevice device)
    {
        if (m_context)
        {
            ImGui_ImplVulkan_Shutdown();

            ImGui::DestroyContext(m_context);
            m_context = nullptr;
        }

        if (m_descriptorPool)
        {
            vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
            m_descriptorPool = nullptr;
        }

        destroy_dummy_pipeline(device);
    }

    void imgui::process(const SDL_Event& event)
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
    }

    void imgui::begin_frame()
    {
        OBLO_ASSERT(m_context);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        ImGui::NewFrame();
    }

    namespace
    {
        void read_imgui_vulkan_state(VkPipelineLayout* pipelineLayout,
                                     VkShaderModule* vertexShaderModule,
                                     VkShaderModule* fragmentShaderModule)
        {
            static_assert(IMGUI_VERSION_NUM == 18700, "This struct should be checked in case of version update");

            struct ImGui_ImplVulkan_Data_Header
            {
                ImGui_ImplVulkan_InitInfo VulkanInitInfo;
                VkRenderPass RenderPass;
                VkDeviceSize BufferMemoryAlignment;
                VkPipelineCreateFlags PipelineCreateFlags;
                VkDescriptorSetLayout DescriptorSetLayout;
                VkPipelineLayout PipelineLayout;
                VkPipeline Pipeline;
                uint32_t Subpass;
                VkShaderModule ShaderModuleVert;
                VkShaderModule ShaderModuleFrag;
            };

            ImGui_ImplVulkan_Data_Header header;

            const auto* userdata = ImGui::GetIO().BackendRendererUserData;
            std::memcpy(&header, userdata, sizeof(header));

            *pipelineLayout = header.PipelineLayout;
            *vertexShaderModule = header.ShaderModuleVert;
            *fragmentShaderModule = header.ShaderModuleFrag;
        }
    }

    void imgui::end_frame(VkCommandBuffer commandBuffer, VkImageView imageView, u32 width, u32 height)
    {
        ImGui::Render();

        ImDrawData* drawData = ImGui::GetDrawData();

        const VkRenderingAttachmentInfo colorAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = imageView,
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        const VkRenderingInfo renderInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {.extent{.width = width, .height = height}},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
        };

        // For some reason, alpha blending doesn't seem to work with imgui when using the dynamic rendering
        // extension
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_dummyPipeline);

        vkCmdBeginRendering(commandBuffer, &renderInfo);
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
        vkCmdEndRendering(commandBuffer);
    }

    bool imgui::create_dummy_pipeline(VkDevice device)
    {
        VkPipelineLayout pipelineLayout;
        VkShaderModule vertexShaderModule, fragmentShaderModule;

        read_imgui_vulkan_state(&pipelineLayout, &vertexShaderModule, &fragmentShaderModule);

        const VkPipelineShaderStageCreateInfo vertShaderStageInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexShaderModule,
            .pName = "main"};

        const VkPipelineShaderStageCreateInfo fragShaderStageInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShaderModule,
            .pName = "main"};

        const VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertShaderStageInfo,
            fragShaderStageInfo,
        };

        constexpr VkVertexInputBindingDescription bindingDesc{
            .stride = sizeof(ImDrawVert),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };

        const VkVertexInputAttributeDescription vertexInputAttribute[] = {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = IM_OFFSETOF(ImDrawVert, pos),
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = IM_OFFSETOF(ImDrawVert, uv),
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .offset = IM_OFFSETOF(ImDrawVert, col),
            },
        };

        const VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDesc,
            .vertexAttributeDescriptionCount = 3,
            .pVertexAttributeDescriptions = vertexInputAttribute,
        };

        const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE};

        const VkViewport viewport{.x = 0.0f,
                                  .y = 0.0f,
                                  .width = 1.f,
                                  .height = 1.f,
                                  .minDepth = 0.0f,
                                  .maxDepth = 1.0f};

        const VkRect2D scissor{.extent = {.width = 1, .height = 1}};

        const VkPipelineViewportStateCreateInfo viewportState{.sType =
                                                                  VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                              .viewportCount = 1,
                                                              .pViewports = &viewport,
                                                              .scissorCount = 1,
                                                              .pScissors = &scissor};

        const VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.f};

        const VkPipelineMultisampleStateCreateInfo multisampling{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .minSampleShading = 1.f};

        const VkPipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT};

        const VkPipelineColorBlendStateCreateInfo colorBlending{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
            .blendConstants = {0.f}};

        constexpr auto swapchainFormat{VK_FORMAT_B8G8R8A8_UNORM};

        const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapchainFormat,
        };

        const VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &pipelineRenderingCreateInfo,
            .stageCount = array_size(shaderStages),
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &colorBlending,
            .pDynamicState = nullptr,
            .layout = pipelineLayout,
            .renderPass = nullptr,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1,
        };

        return vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_dummyPipeline) ==
               VK_SUCCESS;
    }

    void imgui::destroy_dummy_pipeline(VkDevice device)
    {
        if (m_dummyPipeline)
        {
            vkDestroyPipeline(device, m_dummyPipeline, nullptr);
            m_dummyPipeline = nullptr;
        }
    }
}