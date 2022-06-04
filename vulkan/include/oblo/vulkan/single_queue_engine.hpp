#pragma once

#include <oblo/core/types.hpp>
#include <span>
#include <vulkan/vulkan.h>

namespace oblo::vk
{
    class single_queue_engine
    {
    public:
        single_queue_engine() = default;
        single_queue_engine(const single_queue_engine&) = delete;
        single_queue_engine(single_queue_engine&&);
        single_queue_engine& operator=(const single_queue_engine&) = delete;
        single_queue_engine& operator=(single_queue_engine&&);
        ~single_queue_engine();

        bool init(VkInstance instance,
                  VkSurfaceKHR surface,
                  std::span<const char* const> enabledLayers,
                  std::span<const char* const> enabledExtensions);

        bool create_swapchain(VkSurfaceKHR surface, u32 width, u32 height, VkFormat format, u32 imageCount);

        VkPhysicalDevice get_physical_device() const;
        VkDevice get_device() const;
        VkQueue get_queue() const;
        VkSwapchainKHR get_swapchain() const;

    private:
        static constexpr u32 MaxSwapChainImageCount{4u};

    private:
        VkPhysicalDevice m_physicalDevice{nullptr};
        VkDevice m_device{nullptr};
        VkQueue m_queue{nullptr};
        VkSwapchainKHR m_swapchain{nullptr};
        VkImage m_images[MaxSwapChainImageCount]{nullptr};
    };
}