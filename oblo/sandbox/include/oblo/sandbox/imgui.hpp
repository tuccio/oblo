#pragma once

#include <oblo/core/types.hpp>
#include <oblo/sandbox/sandbox_app_config.hpp>
#include <vulkan/vulkan.h>

struct ImGuiContext;
struct SDL_Window;
union SDL_Event;

using ImGuiContextFlags = int;

namespace oblo::vk
{
    class imgui
    {
    public:
        bool fill_init_command_buffer(SDL_Window* window,
            VkInstance,
            VkPhysicalDevice physicalDevice,
            VkDevice device,
            VkQueue queue,
            u32 swapchainImageCount,
            VkFormat swapChainFormat,
            const sandbox_app_config& config);

        void shutdown(VkDevice device);

        void process(const SDL_Event& event);
        void begin_frame();
        void end_frame(VkCommandBuffer commandBuffer, VkImageView imageView, u32 width, u32 height);

        bool is_capturing_mouse() const;
        bool is_capturing_keyboard() const;

    private:
        ImGuiContext* m_context{nullptr};
        VkDescriptorPool m_descriptorPool{nullptr};
    };
}