#include <sandbox/imgui.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/size.hpp>
#include <oblo/vulkan/error.hpp>

#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_vulkan.h>

#include <SDL.h>
#include <imgui.h>

namespace oblo::vk
{
    bool imgui::init(SDL_Window* window,
                     VkInstance instance,
                     VkPhysicalDevice physicalDevice,
                     VkDevice device,
                     VkQueue queue,
                     VkCommandBuffer commandBuffer,
                     u32 swapchainImageCount)
    {
        if (m_context)
        {
            return false;
        }

        constexpr auto poolSize{1000};

        constexpr VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, poolSize},
                                                      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, poolSize},
                                                      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, poolSize},
                                                      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, poolSize},
                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, poolSize},
                                                      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, poolSize},
                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, poolSize},
                                                      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, poolSize},
                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, poolSize},
                                                      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, poolSize},
                                                      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, poolSize}};

        const VkDescriptorPoolCreateInfo poolInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                     .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                                     .maxSets = poolSize,
                                                     .poolSizeCount = size(poolSizes),
                                                     .pPoolSizes = poolSizes};

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
        {
            return false;
        }

        m_context = ImGui::CreateContext();

        ImGui_ImplSDL2_InitForVulkan(window);

        ImGui_ImplVulkan_InitInfo initInfo{.Instance = instance,
                                           .PhysicalDevice = physicalDevice,
                                           .Device = device,
                                           .Queue = queue,
                                           .DescriptorPool = m_descriptorPool,
                                           .MinImageCount = swapchainImageCount,
                                           .ImageCount = swapchainImageCount,
                                           .MSAASamples = VK_SAMPLE_COUNT_1_BIT};

        if (ImGui_ImplVulkan_Init(&initInfo, nullptr))
        {
            const VkCommandBufferBeginInfo beginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

            OBLO_VK_PANIC(vkBeginCommandBuffer(commandBuffer, &beginInfo));

            bool success = ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);

            const VkSubmitInfo submitInfo = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                             .commandBufferCount = 1,
                                             .pCommandBuffers = &commandBuffer};

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

    void imgui::end_frame(VkCommandBuffer commandBuffer, VkImageView imageView, u32 width, u32 height)
    {
        ImGui::ShowDemoWindow();

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

        vkCmdBeginRendering(commandBuffer, &renderInfo);
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
        vkCmdEndRendering(commandBuffer);
    }
}