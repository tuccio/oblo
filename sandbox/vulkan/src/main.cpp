#include <oblo/core/types.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/instance.hpp>
#include <oblo/vulkan/shader_compiler.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

enum class error
{
    success,
    create_window,
    create_surface,
    create_device,
    create_swapchain,
    create_command_buffers,
    create_pipeline,
    compile_shaders
};

namespace
{
    struct extensions
    {
        PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
        PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;
    };

    [[maybe_unused]] extensions load_extensions(VkInstance instance)
    {
        auto vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());

        extensions res{};

        res.vkCmdBeginRenderingKHR =
            reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetInstanceProcAddr(instance, "vkCmdBeginRenderingKHR"));

        res.vkCmdEndRenderingKHR =
            reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetInstanceProcAddr(instance, "vkCmdEndRenderingKHR"));

        return res;
    }

    struct config
    {
        bool vk_use_validation_layers{false};
    };

#define OBLO_READ_VAR_(Var)                                                                                            \
    if (json.count(#Var) > 0)                                                                                          \
        cfg.Var = json.at(#Var).get<decltype(config::Var)>();

    config load_config(const std::filesystem::path& path)
    {
        config cfg{};
        std::ifstream ifs{path};

        if (ifs.is_open())
        {
            const auto json = nlohmann::json::parse(ifs);
            OBLO_READ_VAR_(vk_use_validation_layers);
        }

        return cfg;
    }
#undef OBLO_READ_VAR_

    VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback([[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  [[maybe_unused]] void* pUserData)
    {
        fprintf(stderr, "[Vulkan Validation] (%x) %s\n", messageType, pCallbackData->pMessage);
        return VK_FALSE;
    }

    int run(SDL_Window* window)
    {
        using namespace oblo;

        const auto config = load_config("vksandbox.json");

        constexpr u32 swapchainImages{2u};
        constexpr auto swapchainFormat{VK_FORMAT_B8G8R8A8_UNORM};

        error returnCode{error::success};

        vk::instance instance;
        vk::single_queue_engine engine;
        vk::command_buffer_pool pools[swapchainImages];

        u32 renderWidth, renderHeight;

        VkSemaphore presentSemaphore;
        VkSemaphore timelineSemaphore;
        VkFence presentFences[swapchainImages];
        VkFramebuffer frameBuffers[swapchainImages];

        VkShaderModule vertShaderModule{nullptr};
        VkShaderModule fragShaderModule{nullptr};
        VkPipelineLayout pipelineLayout{nullptr};
        VkPipeline graphicsPipeline{nullptr};
        VkRenderPass renderPass{nullptr};

        vk::shader_compiler shaderCompiler;

        u64 frameSemaphoreValues[swapchainImages] = {0};
        u64 currentSemaphoreValue{0};

        {
            // We need to gather the extensions needed by SDL, for now we hardcode a max number
            constexpr u32 extensionsArraySize{64};
            constexpr u32 layersArraySize{16};
            const char* extensions[extensionsArraySize];
            const char* layers[layersArraySize];

            u32 extensionsCount = extensionsArraySize;
            u32 layersCount = 0;

            if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionsCount, extensions))
            {
                returnCode = error::create_device;
                goto done;
            }

            if (config.vk_use_validation_layers)
            {
                layers[layersCount++] = "VK_LAYER_KHRONOS_validation";
            }

            constexpr u32 apiVersion{VK_API_VERSION_1_2};
            constexpr const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME};

            if (!instance.init(
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
                returnCode = error::create_device;
                goto done;
            }

            VkSurfaceKHR surface{nullptr};

            if (!SDL_Vulkan_CreateSurface(window, instance.get(), &surface))
            {
                returnCode = error::create_surface;
                goto done;
            }

            const VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeature{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
                .pNext = nullptr,
                .timelineSemaphore = VK_TRUE};

            if (!engine.init(instance.get(), std::move(surface), {}, deviceExtensions, &timelineFeature))
            {
                returnCode = error::create_device;
                goto done;
            }

            int width, height;
            SDL_Vulkan_GetDrawableSize(window, &width, &height);

            renderWidth = u32(width);
            renderHeight = u32(height);

            if (!engine.create_swapchain(surface, renderWidth, renderHeight, swapchainFormat, swapchainImages))
            {
                returnCode = error::create_swapchain;
                goto done;
            }

            for (auto& pool : pools)
            {
                if (!pool.init(engine.get_device(), engine.get_queue_family_index(), false, 1, 1))
                {
                    returnCode = error::create_command_buffers;
                    goto done;
                }
            }
        }

        {
            const VkSemaphoreCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                   .pNext = nullptr,
                                                   .flags = 0};

            OBLO_VK_PANIC(vkCreateSemaphore(engine.get_device(), &createInfo, nullptr, &presentSemaphore));
        }

        {
            const VkSemaphoreTypeCreateInfo timelineCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                                               .pNext = nullptr,
                                                               .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                                                               .initialValue = 0};

            const VkSemaphoreCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                   .pNext = &timelineCreateInfo,
                                                   .flags = 0};

            OBLO_VK_PANIC(vkCreateSemaphore(engine.get_device(), &createInfo, nullptr, &timelineSemaphore));
        }

        {
            const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                              .pNext = nullptr,
                                              .flags = 0u};

            for (u32 i = 0; i < swapchainImages; ++i)
            {
                OBLO_VK_PANIC(vkCreateFence(engine.get_device(), &fenceInfo, nullptr, &presentFences[i]));
            }
        }

        vertShaderModule = shaderCompiler.create_shader_module_from_glsl_file(engine.get_device(),
                                                                              "./shaders/test.vert",
                                                                              VK_SHADER_STAGE_VERTEX_BIT);

        fragShaderModule = shaderCompiler.create_shader_module_from_glsl_file(engine.get_device(),
                                                                              "./shaders/test.frag",
                                                                              VK_SHADER_STAGE_FRAGMENT_BIT);

        if (!vertShaderModule || !fragShaderModule)
        {
            returnCode = error::compile_shaders;
            goto done;
        }

        {
            const VkAttachmentReference colorAttachmentRef{.attachment = 0,
                                                           .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

            const VkSubpassDescription subpass{.flags = 0u,
                                               .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                               .inputAttachmentCount = 0,
                                               .pInputAttachments = nullptr,
                                               .colorAttachmentCount = 1,
                                               .pColorAttachments = &colorAttachmentRef};

            const VkAttachmentDescription colorAttachment{.flags = 0u,
                                                          .format = swapchainFormat,
                                                          .samples = VK_SAMPLE_COUNT_1_BIT,
                                                          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                          .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

            const VkRenderPassCreateInfo renderPassInfo{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                                        .attachmentCount = 1,
                                                        .pAttachments = &colorAttachment,
                                                        .subpassCount = 1,
                                                        .pSubpasses = &subpass};

            OBLO_VK_PANIC(vkCreateRenderPass(engine.get_device(), &renderPassInfo, nullptr, &renderPass));
        }

        {
            const VkPipelineLayoutCreateInfo pipelineLayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            OBLO_VK_PANIC(vkCreatePipelineLayout(engine.get_device(), &pipelineLayoutInfo, nullptr, &pipelineLayout));
        }

        {
            const VkPipelineShaderStageCreateInfo vertShaderStageInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertShaderModule,
                .pName = "main"};

            const VkPipelineShaderStageCreateInfo fragShaderStageInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragShaderModule,
                .pName = "main"};

            const VkPipelineVertexInputStateCreateInfo vertexInputInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

            const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .primitiveRestartEnable = VK_FALSE};

            const VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

            const VkViewport viewport{.x = 0.0f,
                                      .y = 0.0f,
                                      .width = float(renderWidth),
                                      .height = float(renderHeight),
                                      .minDepth = 0.0f,
                                      .maxDepth = 1.0f};

            const VkRect2D scissor{.offset = {0, 0}, .extent = {.width = renderWidth, .height = renderHeight}};

            const VkPipelineViewportStateCreateInfo viewportState{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
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
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
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

            const VkGraphicsPipelineCreateInfo pipelineInfo{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                            .stageCount = 2,
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
                                                            .renderPass = renderPass,
                                                            .subpass = 0,
                                                            .basePipelineHandle = VK_NULL_HANDLE,
                                                            .basePipelineIndex = -1};

            OBLO_VK_PANIC(vkCreateGraphicsPipelines(engine.get_device(),
                                                    VK_NULL_HANDLE,
                                                    1,
                                                    &pipelineInfo,
                                                    nullptr,
                                                    &graphicsPipeline));
        }

        for (u32 i = 0; i < swapchainImages; ++i)
        {
            const VkImageView attachments[] = {engine.get_image_view(i)};

            const VkFramebufferCreateInfo framebufferInfo{.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                          .pNext = nullptr,
                                                          .flags = 0u,
                                                          .renderPass = renderPass,
                                                          .attachmentCount = std::size(attachments),
                                                          .pAttachments = attachments,
                                                          .width = renderWidth,
                                                          .height = renderHeight,
                                                          .layers = 1};

            OBLO_VK_PANIC(vkCreateFramebuffer(engine.get_device(), &framebufferInfo, nullptr, &frameBuffers[i]));
        }

        for (u64 frameIndex{1};; ++frameIndex)
        {
            for (SDL_Event event; SDL_PollEvent(&event);)
            {
                switch (event.type)
                {
                case SDL_QUIT:
                    goto done;
                }
            }

            const auto currentIndex = frameIndex % swapchainImages;

            OBLO_VK_PANIC(vkGetSemaphoreCounterValue(engine.get_device(), timelineSemaphore, &currentSemaphoreValue));

            if (currentSemaphoreValue < frameSemaphoreValues[currentIndex])
            {
                OBLO_VK_PANIC(vkWaitForFences(engine.get_device(), 1, &presentFences[currentIndex], 0, ~0ull));
            }

            OBLO_VK_PANIC(vkResetFences(engine.get_device(), 1, &presentFences[currentIndex]));

            u32 imageIndex;

            OBLO_VK_PANIC(vkAcquireNextImageKHR(engine.get_device(),
                                                engine.get_swapchain(),
                                                UINT64_MAX,
                                                presentSemaphore,
                                                VK_NULL_HANDLE,
                                                &imageIndex));

            auto& pool = pools[currentIndex];

            pool.reset_pool();
            pool.begin_frame(frameIndex);

            VkCommandBuffer commandBuffer;
            pool.fetch_buffers({&commandBuffer, 1});

            const VkCommandBufferBeginInfo commandBufferBeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                                  .pNext = nullptr,
                                                                  .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                                  .pInheritanceInfo = nullptr};

            OBLO_VK_PANIC(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

            const VkClearValue clearValue{.color = {.float32 = {0.f, 0.f, 0.f, 0.f}}};
            const VkRenderPassBeginInfo renderPassBeginInfo{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = renderPass,
                .framebuffer = frameBuffers[currentIndex],
                .renderArea = {.offset = {0, 0}, .extent = {.width = renderWidth, .height = renderHeight}},
                .clearValueCount = 1,
                .pClearValues = &clearValue};

            vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            vkCmdEndRenderPass(commandBuffer);

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
                                          .pSignalSemaphores = &timelineSemaphore};

            OBLO_VK_PANIC(vkQueueSubmit(engine.get_queue(), 1, &submitInfo, presentFences[currentIndex]));

            auto swapchain = engine.get_swapchain();
            const VkPresentInfoKHR presentInfo = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                                  .pNext = nullptr,
                                                  .waitSemaphoreCount = 1,
                                                  .pWaitSemaphores = &presentSemaphore,
                                                  .swapchainCount = 1,
                                                  .pSwapchains = &swapchain,
                                                  .pImageIndices = &imageIndex,
                                                  .pResults = nullptr};

            OBLO_VK_PANIC(vkQueuePresentKHR(engine.get_queue(), &presentInfo));

            frameSemaphoreValues[currentIndex] = frameIndex;
        }

    done:
        if (engine.get_device())
        {
            vkDeviceWaitIdle(engine.get_device());
        }

        if (graphicsPipeline)
        {
            vkDestroyPipeline(engine.get_device(), graphicsPipeline, nullptr);
        }

        if (pipelineLayout)
        {
            vkDestroyPipelineLayout(engine.get_device(), pipelineLayout, nullptr);
        }

        if (renderPass)
        {
            vkDestroyRenderPass(engine.get_device(), renderPass, nullptr);
        }

        for (auto frameBuffer : frameBuffers)
        {
            vkDestroyFramebuffer(engine.get_device(), frameBuffer, nullptr);
        }

        if (vertShaderModule)
        {
            vkDestroyShaderModule(engine.get_device(), vertShaderModule, nullptr);
        }

        if (fragShaderModule)
        {
            vkDestroyShaderModule(engine.get_device(), fragShaderModule, nullptr);
        }

        for (auto fence : presentFences)
        {
            vkDestroyFence(engine.get_device(), fence, nullptr);
        }

        if (timelineSemaphore)
        {
            vkDestroySemaphore(engine.get_device(), timelineSemaphore, nullptr);
        }

        if (presentSemaphore)
        {
            vkDestroySemaphore(engine.get_device(), presentSemaphore, nullptr);
        }

        return int(returnCode);
    }
}

int SDL_main(int, char*[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    auto* const window = SDL_CreateWindow("Oblo Vulkan Sandbox",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          1280,
                                          720,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

    if (!window)
    {
        return int(error::create_window);
    }

    const auto result = run(window);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return result;
}