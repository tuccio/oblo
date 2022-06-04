#pragma once

#include <span>
#include <vulkan/vulkan.h>

namespace oblo::vk
{
    class single_queue_device
    {
    public:
        single_queue_device() = default;
        single_queue_device(const single_queue_device&) = delete;
        single_queue_device(single_queue_device&&);
        single_queue_device& operator=(const single_queue_device&) = delete;
        single_queue_device& operator=(single_queue_device&&);
        ~single_queue_device();

        bool init(VkInstance instance,
                  VkSurfaceKHR surface,
                  std::span<const char* const> enabledLayers,
                  std::span<const char* const> enabledExtensions);

        VkPhysicalDevice get_physical_device() const;
        VkDevice get_device() const;
        VkQueue get_queue() const;

    private:
        VkPhysicalDevice m_physicalDevice{nullptr};
        VkDevice m_device{nullptr};
        VkQueue m_queue{nullptr};
    };
}