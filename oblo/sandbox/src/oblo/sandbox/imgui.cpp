#include <oblo/sandbox/imgui.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/error.hpp>

#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <IconsFontAwesome6.h>

#include <SDL.h>
#include <imgui.h>

#include <cstring>

namespace oblo::vk
{
    namespace
    {
#include <oblo/sandbox/Archivo-Regular.ttf.h>
#include <oblo/sandbox/fa-solid-900.ttf.h>

        constexpr ImWchar FontAwesome6Ranges[]{ImWchar{ICON_MIN_FA}, ImWchar{ICON_MAX_FA}, ImWchar{}};

        void init_fonts(ImGuiIO& io)
        {
            constexpr f32 textPixels{14.f};
            constexpr f32 bigIconsPixels{48.f};

            ImFontConfig config{};

            config.FontDataOwnedByAtlas = false;

            io.Fonts->AddFontFromMemoryTTF(Archivo_Regular_ttf, Archivo_Regular_ttf_len, textPixels, &config);

            config.MergeMode = true;

            io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf,
                fa_solid_900_ttf_len,
                textPixels,
                &config,
                FontAwesome6Ranges);

            config.MergeMode = false;

            io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf,
                fa_solid_900_ttf_len,
                bigIconsPixels,
                &config,
                FontAwesome6Ranges);
        }
    }

    bool imgui::fill_init_command_buffer(SDL_Window* window,
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkQueue queue,
        u32 swapchainImageCount,
        VkFormat swapChainFormat,
        const sandbox_app_config& config)
    {
        OBLO_PROFILE_SCOPE();

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

        init_fonts(io);

        io.IniFilename = config.imguiIniFile;

        if (config.uiUseDocking)
        {
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        }

        if (config.uiUseMultiViewport)
        {
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        }

        if (config.uiUseKeyboardNavigation)
        {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        }

        ImGui_ImplSDL2_InitForVulkan(window);

        const VkFormat colorAttachmentFormats[] = {swapChainFormat};

        const VkPipelineRenderingCreateInfoKHR pipelineCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = colorAttachmentFormats,
        };

        ImGui_ImplVulkan_InitInfo initInfo{
            .Instance = instance,
            .PhysicalDevice = physicalDevice,
            .Device = device,
            .Queue = queue,
            .DescriptorPool = m_descriptorPool,
            .MinImageCount = swapchainImageCount,
            .ImageCount = swapchainImageCount,
            .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
            .UseDynamicRendering = true,
            .PipelineRenderingCreateInfo = pipelineCreateInfo,
        };

        return ImGui_ImplVulkan_Init(&initInfo) && ImGui_ImplVulkan_CreateFontsTexture();
    }

    void imgui::shutdown(VkDevice device)
    {

        if (m_context)
        {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplSDL2_Shutdown();

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
        OBLO_PROFILE_SCOPE();
        ImGui_ImplSDL2_ProcessEvent(&event);
    }

    void imgui::begin_frame()
    {
        OBLO_PROFILE_SCOPE();
        OBLO_ASSERT(m_context);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        ImGui::NewFrame();
    }

    void imgui::end_frame(VkCommandBuffer commandBuffer, VkImageView imageView, u32 width, u32 height)
    {
        OBLO_PROFILE_SCOPE();
        ImGui::Render();

        auto& io = ImGui::GetIO();

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        const VkRenderingAttachmentInfo colorAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = imageView,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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

        auto* const drawData = ImGui::GetDrawData();

        vkCmdBeginRendering(commandBuffer, &renderInfo);
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
        vkCmdEndRendering(commandBuffer);
    }

    bool imgui::is_capturing_mouse() const
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool imgui::is_capturing_keyboard() const
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }
}