#include <oblo/vulkan/single_queue_engine.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/small_vector.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/instance.hpp>

namespace oblo::vk
{
    single_queue_engine::single_queue_engine(single_queue_engine&& other)
    {
        std::swap(other.m_device, m_device);
        std::swap(other.m_queue, m_queue);
    }

    single_queue_engine& single_queue_engine::operator=(single_queue_engine&& other)
    {
        std::swap(other.m_device, m_device);
        std::swap(other.m_queue, m_queue);
        return *this;
    }

    single_queue_engine::~single_queue_engine()
    {
        destroy_swapchain();

        if (m_device)
        {
            vkDestroyDevice(m_device, nullptr);
        }
    }

    bool single_queue_engine::init(VkInstance instance,
                                   VkSurfaceKHR surface,
                                   std::span<const char* const> enabledLayers,
                                   std::span<const char* const> enabledExtensions,
                                   const void* deviceCreateInfoChain)
    {
        {
            u32 physicalDevicesCount{0u};

            if (vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, nullptr) != VK_SUCCESS ||
                physicalDevicesCount == 0)
            {
                return false;
            }

            small_vector<VkPhysicalDevice, 32> devices;
            devices.resize(physicalDevicesCount);

            if (vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, devices.data()) != VK_SUCCESS)
            {
                return false;
            }

            // TODO: It should actually search for the best GPU and check for API version, but we pick the first
            m_physicalDevice = devices[0];
        }

        constexpr u32 invalid{~0u};
        u32 graphicsQueueFamilyIndex{invalid};

        {
            u32 queueFamilyCount{0u};

            vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);

            small_vector<VkQueueFamilyProperties, 32> queueProperties;
            queueProperties.resize(queueFamilyCount);

            vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueProperties.data());

            for (u32 i = 0; i < queueFamilyCount; ++i)
            {
                if (const auto& properties = queueProperties[i]; properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    VkBool32 presentSupport;
                    vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, surface, &presentSupport);

                    if (presentSupport)
                    {
                        graphicsQueueFamilyIndex = i;
                        break;
                    }
                }
            }

            if (graphicsQueueFamilyIndex == invalid)
            {
                return false;
            }
        }

        constexpr float queuePriorities[] = {1.f};

        const VkDeviceQueueCreateInfo deviceQueueCreateInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                            .pNext = nullptr,
                                                            .flags = 0u,
                                                            .queueFamilyIndex = graphicsQueueFamilyIndex,
                                                            .queueCount = 1u,
                                                            .pQueuePriorities = queuePriorities};

        const VkDeviceCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                            .pNext = deviceCreateInfoChain,
                                            .queueCreateInfoCount = 1,
                                            .pQueueCreateInfos = &deviceQueueCreateInfo,
                                            .enabledLayerCount = u32(enabledLayers.size()),
                                            .ppEnabledLayerNames = enabledLayers.data(),
                                            .enabledExtensionCount = u32(enabledExtensions.size()),
                                            .ppEnabledExtensionNames = enabledExtensions.data(),
                                            .pEnabledFeatures = nullptr};

        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
        {
            return false;
        }

        vkGetDeviceQueue(m_device, graphicsQueueFamilyIndex, 0, &m_queue);
        m_queueFamilyIndex = graphicsQueueFamilyIndex;
        m_instance = instance;

        return true;
    }

    bool single_queue_engine::create_swapchain(
        VkSurfaceKHR surface, u32 width, u32 height, VkFormat format, u32 imageCount)
    {
        if (imageCount > MaxSwapChainImageCount)
        {
            return false;
        }

        VkSurfaceCapabilitiesKHR surfaceCapabilities;

        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, surface, &surfaceCapabilities) != VK_SUCCESS)
        {
            return false;
        }

        small_vector<VkSurfaceFormatKHR, 64> surfaceFormats;
        u32 surfaceFormatsCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, surface, &surfaceFormatsCount, nullptr);
        surfaceFormats.resize(surfaceFormatsCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, surface, &surfaceFormatsCount, surfaceFormats.data());

        const auto surfaceFormatIt =
            std::find_if(surfaceFormats.begin(),
                         surfaceFormats.end(),
                         [format](const VkSurfaceFormatKHR& surfaceFormat) { return surfaceFormat.format == format; });

        if (surfaceFormatIt == surfaceFormats.end())
        {
            return false;
        }

        if (surfaceCapabilities.minImageCount > MaxSwapChainImageCount)
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
                                                     .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                     .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                     .queueFamilyIndexCount = 0,
                                                     .pQueueFamilyIndices = nullptr,
                                                     .preTransform = surfaceCapabilities.currentTransform,
                                                     .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                                     .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                                                     .clipped = VK_TRUE,
                                                     .oldSwapchain = nullptr};

        if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS)
        {
            return false;
        }

        u32 createdImageCount{imageCount};

        if (vkGetSwapchainImagesKHR(m_device, m_swapchain, &createdImageCount, m_images) != VK_SUCCESS ||
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
                .image = m_images[i],
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

            if (vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS)
            {
                return false;
            }
        }

        return true;
    }

    void single_queue_engine::destroy_swapchain()
    {
        if (m_swapchain)
        {
            for (auto*& imageView : m_imageViews)
            {
                if (imageView)
                {
                    vkDestroyImageView(m_device, imageView, nullptr);
                    imageView = nullptr;
                }
            }

            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
            m_swapchain = nullptr;
        }
    }

    VkPhysicalDevice single_queue_engine::get_physical_device() const
    {
        return m_physicalDevice;
    }

    VkDevice single_queue_engine::get_device() const
    {
        return m_device;
    }

    VkQueue single_queue_engine::get_queue() const
    {
        return m_queue;
    }

    u32 single_queue_engine::get_queue_family_index() const
    {
        return m_queueFamilyIndex;
    }

    VkSwapchainKHR single_queue_engine::get_swapchain() const
    {
        return m_swapchain;
    }

    VkImage single_queue_engine::get_image(u32 index) const
    {
        OBLO_ASSERT(index < MaxSwapChainImageCount);
        return m_images[index];
    }

    VkImageView single_queue_engine::get_image_view(u32 index) const
    {
        OBLO_ASSERT(index < MaxSwapChainImageCount);
        return m_imageViews[index];
    }
}