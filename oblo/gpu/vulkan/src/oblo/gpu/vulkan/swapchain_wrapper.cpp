#include <oblo/core/buffered_array.hpp>

#include <oblo/gpu/vulkan/gpu_allocator.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu::vk
{
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
        VkImageView* imageViews)
    {
        VkSurfaceCapabilitiesKHR surfaceCapabilities;

        if (const VkResult r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
            r != VK_SUCCESS)
        {
            return r;
        }

        buffered_array<VkSurfaceFormatKHR, 64> surfaceFormats;
        u32 surfaceFormatsCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatsCount, nullptr);
        surfaceFormats.resize(surfaceFormatsCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatsCount, surfaceFormats.data());

        const auto surfaceFormatIt = std::find_if(surfaceFormats.begin(),
            surfaceFormats.end(),
            [format](const VkSurfaceFormatKHR& surfaceFormat) { return surfaceFormat.format == format; });

        if (surfaceFormatIt == surfaceFormats.end())
        {
            return VK_ERROR_UNKNOWN;
        }

        if (surfaceCapabilities.minImageCount > imageCount)
        {
            return VK_ERROR_UNKNOWN;
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

        auto* const allocationCbs = allocator.get_allocation_callbacks();
        const auto debugUtilsObject = allocator.get_object_debug_utils();

        if (const VkResult r = vkCreateSwapchainKHR(device, &createInfo, allocationCbs, swapchain); r != VK_SUCCESS)
        {
            return r;
        }

        u32 createdImageCount{imageCount};

        if (const VkResult r = vkGetSwapchainImagesKHR(device, *swapchain, &createdImageCount, images); r != VK_SUCCESS)
        {
            return r;
        }

        if (createdImageCount != imageCount)
        {
            return VK_ERROR_UNKNOWN;
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

            if (const VkResult r = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageViews[i]);
                r != VK_SUCCESS)
            {
                return r;
            }

            debugUtilsObject.set_object_name(device, images[i], "Swapchain Image");
            debugUtilsObject.set_object_name(device, imageViews[i], "Swapchain ImageView");
        }

        return VK_SUCCESS;
    }

    void destroy_swapchain(gpu_allocator& allocator,
        VkDevice device,
        VkSwapchainKHR* swapchain,
        VkImage* images,
        VkImageView* imageViews,
        const u32 imageCount)
    {
        if (*swapchain)
        {
            auto* const allocationCbs = allocator.get_allocation_callbacks();

            for (u32 i = 0; i < imageCount; ++i)
            {
                auto& imageView = imageViews[i];
                if (imageView)
                {
                    vkDestroyImageView(device, imageView, allocationCbs);
                    imageView = nullptr;
                }

                images[i] = nullptr;
            }

            vkDestroySwapchainKHR(device, *swapchain, allocationCbs);
            *swapchain = nullptr;
        }
    }
}