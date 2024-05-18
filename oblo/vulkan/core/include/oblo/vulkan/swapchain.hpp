#pragma once

#include <oblo/core/types.hpp>
#include <vulkan/vulkan.h>

namespace oblo::vk
{
    class vulkan_context;

    namespace detail
    {
        bool create_impl(const vulkan_context& ctx,
            VkSurfaceKHR surface,
            u32 width,
            u32 height,
            VkFormat format,
            u32 imageCount,
            VkSwapchainKHR* swapchain,
            VkImage* images,
            VkImageView* imageViews);

        void destroy_impl(const vulkan_context& ctx,
            VkSwapchainKHR* swapchain,
            VkImage* images,
            VkImageView* imageViews,
            u32 imageCount);
    }

    template <u32 SwapChainImageCount>
    class swapchain
    {
    public:
        swapchain() = default;
        swapchain(const swapchain&) = delete;
        swapchain(swapchain&&) noexcept = delete;
        swapchain& operator=(const swapchain&) = delete;
        swapchain& operator=(swapchain&&) noexcept = delete;

        bool create(const vulkan_context& ctx, VkSurfaceKHR surface, u32 width, u32 height, VkFormat format)
        {
            if (m_swapchain)
            {
                return false;
            }

            return detail::create_impl(ctx,
                surface,
                width,
                height,
                format,
                SwapChainImageCount,
                &m_swapchain,
                m_images,
                m_imageViews);
        }

        void destroy(const vulkan_context& ctx)
        {
            detail::destroy_impl(ctx, &m_swapchain, m_images, m_imageViews, SwapChainImageCount);
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

    private:
        friend bool detail::create_impl(const vulkan_context& ctx,
            VkSurfaceKHR surface,
            u32 width,
            u32 height,
            VkFormat format,
            u32 imageCount,
            VkSwapchainKHR* swapchain,
            VkImage* images,
            VkImageView* imageViews);

        friend void detail::destroy_impl(const vulkan_context& ctx,
            VkSwapchainKHR* swapchain,
            VkImage* images,
            VkImageView* imageViews,
            u32 imageCount);

    private:
        VkSwapchainKHR m_swapchain{nullptr};
        VkImage m_images[SwapChainImageCount]{nullptr};
        VkImageView m_imageViews[SwapChainImageCount]{nullptr};
    };
}