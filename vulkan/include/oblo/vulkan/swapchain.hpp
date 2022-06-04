#pragma once

#include <oblo/core/types.hpp>
#include <vulkan/vulkan.h>

namespace oblo::vk
{
    class swapchain
    {
    public:
        swapchain() = default;
        swapchain(const swapchain&) = delete;
        swapchain(swapchain&&) noexcept;
        ~swapchain();

        swapchain& operator=(const swapchain&) = delete;
        swapchain& operator=(swapchain&&) noexcept;

        bool init(VkPhysicalDevice physicalDevice,
                  VkDevice device,
                  VkSurfaceKHR surface,
                  u32 width,
                  u32 height,
                  VkFormat format,
                  u32 imageCount);

    private:
        static constexpr u32 MaxImageCount{4u};

    private:
        VkDevice m_device{nullptr};
        VkSwapchainKHR m_swapchain{nullptr};
        VkImage m_images[MaxImageCount]{nullptr};
    };
}