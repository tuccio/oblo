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
            VkCommandBuffer commandBuffer,
            u32 swapchainImageCount,
            const sandbox_app_config& config);

        void finalize_init(VkDevice device);

        void shutdown(VkDevice device);

        void process(const SDL_Event& event);
        void begin_frame();
        void end_frame(VkCommandBuffer commandBuffer, VkImageView imageView, u32 width, u32 height);

    private:
        ImGuiContext* m_context{nullptr};
        VkDescriptorPool m_descriptorPool{nullptr};
    };
}