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
        if (m_device)
        {
            vkDestroyDevice(m_device, nullptr);
        }
    }

    bool single_queue_engine::init(VkInstance instance,
                                   VkSurfaceKHR surface,
                                   std::span<const char* const> enabledLayers,
                                   std::span<const char* const> enabledExtensions,
                                   const void* deviceCreateInfoChain,
                                   const VkPhysicalDeviceFeatures* physicalDeviceFeatures)
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
        u32 graphicsQueueFamilyIndex{0u};

        if (surface)
        {
            // When a surface is provider, we look for a queue with present support
            graphicsQueueFamilyIndex = invalid;

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
                                            .pEnabledFeatures = physicalDeviceFeatures};

        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
        {
            return false;
        }

        vkGetDeviceQueue(m_device, graphicsQueueFamilyIndex, 0, &m_queue);
        m_queueFamilyIndex = graphicsQueueFamilyIndex;
        m_instance = instance;

        return true;
    }

    void single_queue_engine::shutdown()
    {
        if (m_device)
        {
            vkDestroyDevice(m_device, nullptr);
            m_device = nullptr;
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
}