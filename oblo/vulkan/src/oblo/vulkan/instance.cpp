#include <oblo/vulkan/instance.hpp>

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
        if (m_instance)
        {
            vkDestroyInstance(m_instance, nullptr);
        }
    }

    bool instance::init(const VkApplicationInfo& app,
                        std::span<const char* const> enabledLayers,
                        std::span<const char* const> enabledExtensions,
                        PFN_vkDebugUtilsMessengerCallbackEXT debugCallback,
                        void* debugCallbackUserdata)

    {
        OBLO_ASSERT(!m_instance);

        VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};

        if (debugCallback)
        {
            debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

            constexpr bool verbose = false;

            debugMessengerCreateInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

            if constexpr (verbose)
            {
                debugMessengerCreateInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
            }

            debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugMessengerCreateInfo.pfnUserCallback = debugCallback;
            debugMessengerCreateInfo.pUserData = debugCallbackUserdata;
        }

        const VkInstanceCreateInfo instanceInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = debugCallback ? &debugMessengerCreateInfo : nullptr,
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