#include <oblo/vulkan/swapchain.hpp>

#include <oblo/core/small_vector.hpp>
#include <oblo/vulkan/error.hpp>

namespace oblo::vk
{
    swapchain::swapchain(swapchain&& other) noexcept
    {
        std::swap(other.m_swapchain, m_swapchain);
        std::swap(other.m_device, m_device);
    }

    swapchain::~swapchain()
    {
        if (m_swapchain)
        {
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        }
    }

    swapchain& swapchain::operator=(swapchain&& other) noexcept
    {
        std::swap(other.m_swapchain, m_swapchain);
        std::swap(other.m_device, m_device);
        return *this;
    }

    bool swapchain::init(VkPhysicalDevice physicalDevice,
                         VkDevice device,
                         VkSurfaceKHR surface,
                         u32 width,
                         u32 height,
                         VkFormat format,
                         u32 imageCount)
    {
        if (imageCount > MaxImageCount)
        {
            return false;
        }

        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        OBLO_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));

        small_vector<VkSurfaceFormatKHR, 64> surfaceFormats;
        u32 surfaceFormatsCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatsCount, nullptr);
        surfaceFormats.resize(surfaceFormatsCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatsCount, surfaceFormats.data());

        const auto surfaceFormatIt =
            std::find_if(surfaceFormats.begin(),
                         surfaceFormats.end(),
                         [format](const VkSurfaceFormatKHR& surfaceFormat) { return surfaceFormat.format == format; });

        if (surfaceFormatIt == surfaceFormats.end())
        {
            return false;
        }

        if (surfaceCapabilities.minImageCount > MaxImageCount)
        {
            return false;
        }

        m_device = device;

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
                                                     .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                     .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                     .queueFamilyIndexCount = 0,
                                                     .pQueueFamilyIndices = nullptr,
                                                     .preTransform = surfaceCapabilities.currentTransform,
                                                     .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                                     .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                                                     .clipped = VK_TRUE,
                                                     .oldSwapchain = nullptr};

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS)
        {
            return false;
        }

        u32 createdImageCount;
        OBLO_VK_CHECK(vkGetSwapchainImagesKHR(device, m_swapchain, &createdImageCount, m_images));

        return createdImageCount == imageCount;
    }
}