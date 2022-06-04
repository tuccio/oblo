#pragma once

#include <span>
#include <vulkan/vulkan.h>

namespace oblo::vk
{
    class instance
    {
    public:
        instance() = default;
        instance(const instance&) = delete;
        instance(instance&&) noexcept;
        ~instance();

        instance& operator=(const instance&) = delete;
        instance& operator=(instance&&) noexcept;

        bool init(const VkApplicationInfo& app,
                  std::span<const char* const> enabledLayers,
                  std::span<const char* const> enabledExtensions);

        VkInstance get() const;

    private:
        VkInstance m_instance{nullptr};
    };
}