#include <sandbox/sandbox_app.hpp>

#include <oblo/core/size.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/vulkan/error.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <fstream>
#include <span>
#include <vector>

#include <nlohmann/json.hpp>

#define OBLO_READ_CFG_VAR(Config, Var)                                                                                 \
    if (json.count(#Var) > 0)                                                                                          \
        Config.Var = json.at(#Var).get<decltype(config::Var)>();

namespace oblo::vk
{
    namespace
    {
        VKAPI_ATTR VkBool32 VKAPI_CALL
        debugCallback([[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                      [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
                      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                      [[maybe_unused]] void* pUserData)
        {
            fprintf(stderr, "[Vulkan Validation] (%x) %s\n", messageType, pCallbackData->pMessage);
            return VK_FALSE;
        }

        constexpr auto SwapchainFormat{VK_FORMAT_B8G8R8A8_UNORM};
    }

    sandbox_app::~sandbox_app()
    {
        if (m_engine.get_device())
        {
            m_imgui.shutdown(m_engine.get_device());

            m_allocator.destroy(m_positions);
            m_allocator.destroy(m_colors);

            destroy_device_objects_checked(m_engine.get_device(),
                                           m_graphicsPipeline,
                                           m_pipelineLayout,
                                           m_vertShaderModule,
                                           m_fragShaderModule,
                                           std::span{m_presentFences},
                                           m_timelineSemaphore,
                                           m_presentSemaphore);

            m_swapchain.destroy(m_engine);
        }

        if (m_surface)
        {
            vkDestroySurfaceKHR(m_instance.get(), m_surface, nullptr);
        }

        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }
    }

    bool sandbox_app::init()
    {
        load_config();

        if (!create_window() || !create_engine() ||
            !m_allocator.init(m_instance.get(), m_engine.get_physical_device(), m_engine.get_device()))
        {
            return false;
        }

        int width, height;
        SDL_Vulkan_GetDrawableSize(m_window, &width, &height);

        m_renderWidth = u32(width);
        m_renderHeight = u32(height);

        if (!create_swapchain())
        {
            return false;
        }

        return create_command_pools() && create_synchronization_objects() && create_shader_modules() &&
               create_graphics_pipeline();
    }

    void sandbox_app::run()
    {
        if (!create_vertex_buffers())
        {
            return;
        }

        {
            auto& pool = m_pools[0];
            constexpr auto frameIndex{0u};
            pool.begin_frame(frameIndex);

            VkCommandBuffer imguiUploadCmdBuffer;
            pool.fetch_buffers({&imguiUploadCmdBuffer, 1});

            if (!m_imgui.init(m_window,
                              m_instance.get(),
                              m_engine.get_physical_device(),
                              m_engine.get_device(),
                              m_engine.get_queue(),
                              imguiUploadCmdBuffer,
                              SwapchainImages))
            {
                return;
            }

            pool.reset_buffers(frameIndex);
            pool.reset_pool();
        }

        for (u64 frameIndex{1};; ++frameIndex)
        {
            for (SDL_Event event; SDL_PollEvent(&event);)
            {
                switch (event.type)
                {
                case SDL_QUIT:
                    return;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        wait_idle();

                        m_swapchain.destroy(m_engine);

                        m_renderWidth = u32(event.window.data1);
                        m_renderHeight = u32(event.window.data2);

                        if (!create_swapchain())
                        {
                            return;
                        }
                    }

                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.scancode == SDL_SCANCODE_F2)
                    {
                        m_showImgui = !m_showImgui;
                    }
                }

                if (m_showImgui)
                {
                    m_imgui.process(event);
                }
            }

            const auto poolIndex = frameIndex % SwapchainImages;

            OBLO_VK_PANIC(
                vkGetSemaphoreCounterValue(m_engine.get_device(), m_timelineSemaphore, &m_currentSemaphoreValue));

            if (m_currentSemaphoreValue < m_frameSemaphoreValues[poolIndex])
            {
                OBLO_VK_PANIC(vkWaitForFences(m_engine.get_device(), 1, &m_presentFences[poolIndex], 0, ~0ull));
            }

            OBLO_VK_PANIC(vkResetFences(m_engine.get_device(), 1, &m_presentFences[poolIndex]));

            u32 imageIndex;

            OBLO_VK_PANIC(vkAcquireNextImageKHR(m_engine.get_device(),
                                                m_swapchain.get(),
                                                UINT64_MAX,
                                                m_presentSemaphore,
                                                VK_NULL_HANDLE,
                                                &imageIndex));

            const auto swapchainImage{m_swapchain.get_image(imageIndex)};
            const auto swapchainImageView{m_swapchain.get_image_view(imageIndex)};

            auto& pool = m_pools[poolIndex];

            pool.reset_pool();
            pool.begin_frame(frameIndex);

            VkCommandBuffer commandBuffer;
            pool.fetch_buffers({&commandBuffer, 1});

            const VkCommandBufferBeginInfo commandBufferBeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                                  .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

            OBLO_VK_PANIC(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

            {
                const VkImageMemoryBarrier imageMemoryBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                              .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                              .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                              .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                              .image = swapchainImage,
                                                              .subresourceRange = {
                                                                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                  .baseMipLevel = 0,
                                                                  .levelCount = 1,
                                                                  .baseArrayLayer = 0,
                                                                  .layerCount = 1,
                                                              }};

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
                .imageView = swapchainImageView,
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            };

            const VkRenderingInfo renderInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {.extent{.width = m_renderWidth, .height = m_renderHeight}},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachmentInfo,
            };

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

            {
                const VkViewport viewport{
                    .width = f32(m_renderWidth),
                    .height = f32(m_renderHeight),
                    .minDepth = 0.f,
                    .maxDepth = 1.f,
                };

                const VkRect2D scissor{.extent{.width = m_renderWidth, .height = m_renderHeight}};

                vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
                vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            }

            vkCmdBeginRendering(commandBuffer, &renderInfo);

            const VkBuffer vertexBuffers[] = {m_positions.buffer, m_colors.buffer};
            constexpr VkDeviceSize offsets[] = {0, 0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            vkCmdEndRendering(commandBuffer);

            if (m_showImgui)
            {
                m_imgui.begin_frame();
                m_imgui.end_frame(commandBuffer, swapchainImageView, m_renderWidth, m_renderHeight);
            }

            {
                const VkImageMemoryBarrier imageMemoryBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                              .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                              .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                              .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                              .image = swapchainImage,
                                                              .subresourceRange = {
                                                                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                  .baseMipLevel = 0,
                                                                  .levelCount = 1,
                                                                  .baseArrayLayer = 0,
                                                                  .layerCount = 1,
                                                              }};

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

            OBLO_VK_PANIC(vkEndCommandBuffer(commandBuffer));

            const VkTimelineSemaphoreSubmitInfo timelineInfo{.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
                                                             .pNext = nullptr,
                                                             .waitSemaphoreValueCount = 0,
                                                             .pWaitSemaphoreValues = nullptr,
                                                             .signalSemaphoreValueCount = 1,
                                                             .pSignalSemaphoreValues = &frameIndex};

            const VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                          .pNext = &timelineInfo,
                                          .waitSemaphoreCount = 0,
                                          .pWaitSemaphores = nullptr,
                                          .commandBufferCount = 1,
                                          .pCommandBuffers = &commandBuffer,
                                          .signalSemaphoreCount = 1,
                                          .pSignalSemaphores = &m_timelineSemaphore};

            OBLO_VK_PANIC(vkQueueSubmit(m_engine.get_queue(), 1, &submitInfo, m_presentFences[poolIndex]));

            const auto swapchain = m_swapchain.get();
            const VkPresentInfoKHR presentInfo = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                                  .pNext = nullptr,
                                                  .waitSemaphoreCount = 1,
                                                  .pWaitSemaphores = &m_presentSemaphore,
                                                  .swapchainCount = 1,
                                                  .pSwapchains = &swapchain,
                                                  .pImageIndices = &imageIndex,
                                                  .pResults = nullptr};

            OBLO_VK_PANIC(vkQueuePresentKHR(m_engine.get_queue(), &presentInfo));

            m_frameSemaphoreValues[poolIndex] = frameIndex;
        }
    }

    void sandbox_app::wait_idle()
    {
        vkDeviceWaitIdle(m_engine.get_device());
    }

    void sandbox_app::load_config()
    {
        m_config = {};
        std::ifstream ifs{"vksandbox.json"};

        if (ifs.is_open())
        {
            const auto json = nlohmann::json::parse(ifs);
            OBLO_READ_CFG_VAR(m_config, vk_use_validation_layers);
        }
    }

    bool sandbox_app::create_window()
    {
        m_window = SDL_CreateWindow("Oblo Vulkan Sandbox",
                                    SDL_WINDOWPOS_UNDEFINED,
                                    SDL_WINDOWPOS_UNDEFINED,
                                    1280,
                                    720,
                                    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

        return m_window != nullptr;
    }

    bool sandbox_app::create_engine()
    {
        // We need to gather the extensions needed by SDL, for now we hardcode a max number
        constexpr u32 extensionsArraySize{64};
        constexpr u32 layersArraySize{16};
        const char* extensions[extensionsArraySize];
        const char* layers[layersArraySize];

        u32 extensionsCount = extensionsArraySize;
        u32 layersCount = 0;

        if (!SDL_Vulkan_GetInstanceExtensions(m_window, &extensionsCount, extensions))
        {
            return false;
        }

        if (m_config.vk_use_validation_layers)
        {
            layers[layersCount++] = "VK_LAYER_KHRONOS_validation";
        }

        constexpr u32 apiVersion{VK_API_VERSION_1_3};
        constexpr const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
                                                    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME};

        if (!m_instance.init(
                VkApplicationInfo{
                    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                    .pNext = nullptr,
                    .pApplicationName = "vksandbox",
                    .applicationVersion = 0,
                    .pEngineName = "oblo",
                    .engineVersion = 0,
                    .apiVersion = apiVersion,
                },
                {layers, layersCount},
                {extensions, extensionsCount},
                debugCallback))
        {
            return false;
        }

        if (!SDL_Vulkan_CreateSurface(m_window, m_instance.get(), &m_surface))
        {
            return false;
        }

        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .dynamicRendering = VK_TRUE};

        VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeature{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
            .pNext = &dynamicRenderingFeature,
            .timelineSemaphore = VK_TRUE};

        return m_engine.init(m_instance.get(), m_surface, {}, deviceExtensions, &timelineFeature);
    }

    bool sandbox_app::create_swapchain()
    {
        return m_swapchain.create(m_engine, m_surface, m_renderWidth, m_renderHeight, SwapchainFormat);
    }

    bool sandbox_app::create_command_pools()
    {
        for (auto& pool : m_pools)
        {
            if (!pool.init(m_engine.get_device(), m_engine.get_queue_family_index(), false, 1, 1))
            {
                return false;
            }
        }

        return true;
    }

    bool sandbox_app::create_synchronization_objects()
    {
        const VkSemaphoreCreateInfo presentSemaphoreCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                               .pNext = nullptr,
                                                               .flags = 0};

        const VkSemaphoreTypeCreateInfo timelineTypeCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                                               .pNext = nullptr,
                                                               .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                                                               .initialValue = 0};

        const VkSemaphoreCreateInfo timelineSemaphoreCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                                .pNext = &timelineTypeCreateInfo,
                                                                .flags = 0};

        const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0u};

        for (u32 i = 0; i < SwapchainImages; ++i)
        {
            if (vkCreateFence(m_engine.get_device(), &fenceInfo, nullptr, &m_presentFences[i]) != VK_SUCCESS)
            {
                return false;
            }
        }

        return vkCreateSemaphore(m_engine.get_device(), &presentSemaphoreCreateInfo, nullptr, &m_presentSemaphore) ==
                   VK_SUCCESS &&
               vkCreateSemaphore(m_engine.get_device(), &timelineSemaphoreCreateInfo, nullptr, &m_timelineSemaphore) ==
                   VK_SUCCESS;
    }

    bool sandbox_app::create_shader_modules()
    {
        m_vertShaderModule = m_shaderCompiler.create_shader_module_from_glsl_file(m_engine.get_device(),
                                                                                  "./shaders/test.vert",
                                                                                  VK_SHADER_STAGE_VERTEX_BIT);

        m_fragShaderModule = m_shaderCompiler.create_shader_module_from_glsl_file(m_engine.get_device(),
                                                                                  "./shaders/test.frag",
                                                                                  VK_SHADER_STAGE_FRAGMENT_BIT);

        return m_vertShaderModule && m_fragShaderModule;
    }

    bool sandbox_app::create_graphics_pipeline()
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

        if (vkCreatePipelineLayout(m_engine.get_device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) !=
            VK_SUCCESS)
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
            .vertexBindingDescriptionCount = std::size(vertexInputBindingDescs),
            .pVertexBindingDescriptions = vertexInputBindingDescs,
            .vertexAttributeDescriptionCount = std::size(vertexInputAttributeDescs),
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
            .pColorAttachmentFormats = &SwapchainFormat,
        };

        constexpr VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        const VkPipelineDynamicStateCreateInfo dynamicState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = size(dynamicStates),
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

        return vkCreateGraphicsPipelines(m_engine.get_device(),
                                         VK_NULL_HANDLE,
                                         1,
                                         &pipelineInfo,
                                         nullptr,
                                         &m_graphicsPipeline) == VK_SUCCESS;
    }

    bool sandbox_app::create_vertex_buffers()
    {
        constexpr vec2 positions[] = {{0.0f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
        constexpr vec3 colors[] = {{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

        if (m_allocator.create_buffer({.size = sizeof(positions),
                                       .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                       .memoryUsage = memory_usage::cpu_to_gpu},
                                      m_positions) != VK_SUCCESS ||
            m_allocator.create_buffer({.size = sizeof(colors),
                                       .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                       .memoryUsage = memory_usage::cpu_to_gpu},
                                      m_colors) != VK_SUCCESS)
        {
            return false;
        }

        if (void* data; m_allocator.map(m_positions.allocation, &data) == VK_SUCCESS)
        {
            std::memcpy(data, positions, sizeof(positions));
            m_allocator.unmap(m_positions.allocation);
        }

        if (void* data; m_allocator.map(m_colors.allocation, &data) == VK_SUCCESS)
        {
            std::memcpy(data, colors, sizeof(colors));
            m_allocator.unmap(m_colors.allocation);
        }

        return true;
    }

    void sandbox_app::destroy_graphics_pipeline()
    {
        if (m_pipelineLayout)
        {
            vkDestroyPipelineLayout(m_engine.get_device(), m_pipelineLayout, nullptr);
            m_pipelineLayout = nullptr;
        }

        if (m_graphicsPipeline)
        {
            vkDestroyPipeline(m_engine.get_device(), m_graphicsPipeline, nullptr);
            m_graphicsPipeline = nullptr;
        }
    }
}