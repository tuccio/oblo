#pragma once

#include <oblo/core/types.hpp>
#include <vulkan/vulkan.h>

union SDL_Event;
struct SDL_Window;
struct ImGuiContext;

namespace oblo::vk
{
    class imgui
    {
    public:
        bool init(SDL_Window* window,
                  VkInstance,
                  VkPhysicalDevice physicalDevice,
                  VkDevice device,
                  VkQueue queue,
                  VkCommandBuffer commandBuffer,
                  u32 swapchainImageCount);

        void shutdown(VkDevice device);

        void process(const SDL_Event& event);
        void begin_frame();
        void end_frame(VkCommandBuffer commandBuffer, VkImageView imageView, u32 width, u32 height);

    private:
        bool create_dummy_pipeline(VkDevice device);
        void destroy_dummy_pipeline(VkDevice device);

    private:
        ImGuiContext* m_context{nullptr};
        VkDescriptorPool m_descriptorPool{nullptr};
        VkPipeline m_dummyPipeline{nullptr};
    };
}