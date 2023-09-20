#pragma once

#include <oblo/core/types.hpp>
#include <vulkan/vulkan.h>

#include <span>

namespace oblo::vk
{
    class single_queue_engine
    {
    public:
        single_queue_engine() = default;
        single_queue_engine(const single_queue_engine&) = delete;
        single_queue_engine(single_queue_engine&&);
        single_queue_engine& operator=(const single_queue_engine&) = delete;
        single_queue_engine& operator=(single_queue_engine&&);
        ~single_queue_engine();

        bool init(VkInstance instance,
                  VkSurfaceKHR surface,
                  std::span<const char* const> enabledLayers,
                  std::span<const char* const> enabledExtensions,
                  const void* deviceCreateInfoChain,
                  const VkPhysicalDeviceFeatures* physicalDeviceFeatures);

        void shutdown();

        VkPhysicalDevice get_physical_device() const;
        VkDevice get_device() const;
        VkQueue get_queue() const;
        u32 get_queue_family_index() const;

    private:
        VkPhysicalDevice m_physicalDevice{nullptr};
        VkDevice m_device{nullptr};
        VkQueue m_queue{nullptr};
        u32 m_queueFamilyIndex{~0u};
    };
}