#include <oblo/vulkan/instance.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>

namespace oblo::vk
{
    instance::instance(instance&& other) noexcept
    {
        std::swap(other.m_instance, m_instance);
    }

    instance& instance::operator=(instance&& other) noexcept
    {
        std::swap(other.m_instance, m_instance);
        return *this;
    }

    instance::~instance()
    {
        shutdown();
    }

    bool instance::init(const VkApplicationInfo& app,
        std::span<const char* const> enabledLayers,
        std::span<const char* const> enabledExtensions,
        void* pCreateInfoNext)

    {
        OBLO_ASSERT(!m_instance);

        const VkInstanceCreateInfo instanceInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = pCreateInfoNext,
            .pApplicationInfo = &app,
            .enabledLayerCount = u32(enabledLayers.size()),
            .ppEnabledLayerNames = enabledLayers.data(),
            .enabledExtensionCount = u32(enabledExtensions.size()),
            .ppEnabledExtensionNames = enabledExtensions.data(),
        };

        return vkCreateInstance(&instanceInfo, nullptr, &m_instance) == VK_SUCCESS;
    }

    void instance::shutdown()
    {
        if (m_instance)
        {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = nullptr;
        }
    }

    VkInstance instance::get() const
    {
        return m_instance;
    }

    std::vector<VkLayerProperties> instance::available_layers()
    {
        u32 layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        return availableLayers;
    }
}