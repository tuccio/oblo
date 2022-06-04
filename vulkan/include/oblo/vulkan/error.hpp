#pragma once

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    void panic(const char* file, int line, const char* call, VkResult result);

    constexpr bool failed(VkResult result)
    {
        return result != VK_SUCCESS;
    }
}

#define OBLO_VK_PANIC(Call)                                                                                            \
    if (const VkResult result = Call; result != VK_SUCCESS)                                                            \
    {                                                                                                                  \
        ::oblo::vk::panic(__FILE__, __LINE__, #Call, result);                                                          \
    }
