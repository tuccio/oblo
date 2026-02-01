#pragma once

#include <oblo/core/types.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu::vk
{
    class gpu_allocator;

    VkResult create_swapchain(gpu_allocator& allocator,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkSurfaceKHR surface,
        u32 width,
        u32 height,
        VkFormat format,
        u32 imageCount,
        VkSwapchainKHR* swapchain,
        VkImage* images,
        VkImageView* imageViews);

    void destroy_swapchain(gpu_allocator& allocator,
        VkDevice device,
        VkSwapchainKHR* swapchain,
        VkImage* images,
        VkImageView* imageViews,
        u32 imageCount);

    template <u32 SwapChainImageCount>
    class swapchain
    {
    public:
        swapchain() = default;
        swapchain(const swapchain&) = delete;

        swapchain(swapchain&& other) noexcept
        {
            m_swapchain = other.m_swapchain;
            other.m_swapchain = nullptr;

            for (u32 i = 0; i < SwapChainImageCount; ++i)
            {
                m_images[i] = other.m_images[i];
                m_imageViews[i] = other.m_imageViews[i];
                other.m_images[i] = nullptr;
                other.m_imageViews[i] = nullptr;
            }
        }

        swapchain& operator=(const swapchain&) = delete;

        swapchain& operator=(swapchain&& other) noexcept = delete;

        VkResult create(gpu_allocator& allocator,
            VkPhysicalDevice physicalDevice,
            VkDevice device,
            VkSurfaceKHR surface,
            u32 width,
            u32 height,
            VkFormat format)
        {
            if (m_swapchain)
            {
                return VK_ERROR_UNKNOWN;
            }

            return create_swapchain(allocator,
                physicalDevice,
                device,
                surface,
                width,
                height,
                format,
                SwapChainImageCount,
                &m_swapchain,
                m_images,
                m_imageViews);
        }

        void destroy(gpu_allocator& allocator, VkDevice device)
        {
            destroy_swapchain(allocator, device, &m_swapchain, m_images, m_imageViews, SwapChainImageCount);
        }

        VkSwapchainKHR get() const
        {
            return m_swapchain;
        }

        VkImage get_image(u32 index) const
        {
            return m_images[index];
        }

        VkImageView get_image_view(u32 index) const
        {
            return m_imageViews[index];
        }

        constexpr u32 get_image_count() const
        {
            return SwapChainImageCount;
        }

        explicit operator bool() const
        {
            return m_swapchain != nullptr;
        }

    private:
        VkSwapchainKHR m_swapchain{nullptr};
        VkImage m_images[SwapChainImageCount]{nullptr};
        VkImageView m_imageViews[SwapChainImageCount]{nullptr};
    };
}