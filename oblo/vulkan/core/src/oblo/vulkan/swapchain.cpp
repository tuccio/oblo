#include <oblo/core/small_vector.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>

namespace oblo::vk::detail
{
    bool create_impl(const single_queue_engine& engine,
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

        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(engine.get_physical_device(), surface, &surfaceCapabilities) !=
            VK_SUCCESS)
        {
            return false;
        }

        small_vector<VkSurfaceFormatKHR, 64> surfaceFormats;
        u32 surfaceFormatsCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(engine.get_physical_device(), surface, &surfaceFormatsCount, nullptr);
        surfaceFormats.resize(surfaceFormatsCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(engine.get_physical_device(),
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
        const VkSwapchainCreateInfoKHR createInfo = {.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = surface,
            .minImageCount = surfaceCapabilities.minImageCount,
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
            .oldSwapchain = nullptr};

        if (vkCreateSwapchainKHR(engine.get_device(), &createInfo, nullptr, swapchain) != VK_SUCCESS)
        {
            return false;
        }

        u32 createdImageCount{imageCount};

        if (vkGetSwapchainImagesKHR(engine.get_device(), *swapchain, &createdImageCount, images) != VK_SUCCESS ||
            createdImageCount != imageCount)
        {
            return false;
        }

        for (u32 i = 0; i < imageCount; ++i)
        {
            const VkImageViewCreateInfo imageViewCreateInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0u,
                .image = images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = format,
                .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}};

            if (vkCreateImageView(engine.get_device(), &imageViewCreateInfo, nullptr, &imageViews[i]) != VK_SUCCESS)
            {
                return false;
            }
        }

        return true;
    }

    void destroy_impl(const single_queue_engine& engine,
        VkSwapchainKHR* swapchain,
        VkImage* images,
        VkImageView* imageViews,
        const u32 imageCount)
    {
        if (*swapchain)
        {
            for (u32 i = 0; i < imageCount; ++i)
            {
                auto& imageView = imageViews[i];
                if (imageView)
                {
                    vkDestroyImageView(engine.get_device(), imageView, nullptr);
                    imageView = nullptr;
                }

                images[i] = nullptr;
            }

            vkDestroySwapchainKHR(engine.get_device(), *swapchain, nullptr);
            *swapchain = nullptr;
        }
    }
}