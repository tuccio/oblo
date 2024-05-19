#include <oblo/core/buffered_array.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk::detail
{
    bool create_impl(const vulkan_context& ctx,
        VkSurfaceKHR surface,
        u32 width,
        u32 height,
        VkFormat format,
        u32 imageCount,
        VkSwapchainKHR* swapchain,
        VkImage* images,
        VkImageView* imageViews)
    {
        VkSurfaceCapabilitiesKHR surfaceCapabilities;

        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.get_physical_device(), surface, &surfaceCapabilities) !=
            VK_SUCCESS)
        {
            return false;
        }

        buffered_array<VkSurfaceFormatKHR, 64> surfaceFormats;
        u32 surfaceFormatsCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.get_physical_device(), surface, &surfaceFormatsCount, nullptr);
        surfaceFormats.resize(surfaceFormatsCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.get_physical_device(),
            surface,
            &surfaceFormatsCount,
            surfaceFormats.data());

        const auto surfaceFormatIt = std::find_if(surfaceFormats.begin(),
            surfaceFormats.end(),
            [format](const VkSurfaceFormatKHR& surfaceFormat) { return surfaceFormat.format == format; });

        if (surfaceFormatIt == surfaceFormats.end())
        {
            return false;
        }

        if (surfaceCapabilities.minImageCount > imageCount)
        {
            return false;
        }

        // We assume the graphics and present queue are the same family here
        const VkSwapchainCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormatIt->format,
            .imageColorSpace = surfaceFormatIt->colorSpace,
            .imageExtent = VkExtent2D{.width = width, .height = height},
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = surfaceCapabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .clipped = VK_TRUE,
            .oldSwapchain = nullptr,
        };

        auto& allocator = ctx.get_allocator();
        auto* const allocationCbs = allocator.get_allocation_callbacks();
        const auto debugUtilsObject = allocator.get_object_debug_utils();

        if (vkCreateSwapchainKHR(ctx.get_device(), &createInfo, allocationCbs, swapchain) != VK_SUCCESS)
        {
            return false;
        }

        u32 createdImageCount{imageCount};

        if (vkGetSwapchainImagesKHR(ctx.get_device(), *swapchain, &createdImageCount, images) != VK_SUCCESS ||
            createdImageCount != imageCount)
        {
            return false;
        }

        for (u32 i = 0; i < imageCount; ++i)
        {
            const VkImageViewCreateInfo imageViewCreateInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0u,
                .image = images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = format,
                .components =
                    {
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                    },
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
            };

            if (vkCreateImageView(ctx.get_device(), &imageViewCreateInfo, nullptr, &imageViews[i]) != VK_SUCCESS)
            {
                return false;
            }

            debugUtilsObject.set_object_name(ctx.get_device(), images[i], "Swapchain Image");
            debugUtilsObject.set_object_name(ctx.get_device(), imageViews[i], "Swapchain ImageView");
        }

        return true;
    }

    void destroy_impl(const vulkan_context& ctx,
        VkSwapchainKHR* swapchain,
        VkImage* images,
        VkImageView* imageViews,
        const u32 imageCount)
    {
        if (*swapchain)
        {
            auto& allocator = ctx.get_allocator();
            auto* const allocationCbs = allocator.get_allocation_callbacks();

            for (u32 i = 0; i < imageCount; ++i)
            {
                auto& imageView = imageViews[i];
                if (imageView)
                {
                    vkDestroyImageView(ctx.get_device(), imageView, allocationCbs);
                    imageView = nullptr;
                }

                images[i] = nullptr;
            }

            vkDestroySwapchainKHR(ctx.get_device(), *swapchain, allocationCbs);
            *swapchain = nullptr;
        }
    }
}