#pragma once

#include <vulkan/vulkan.h>

namespace oblo
{
    void vulkan_fatal_error(const char* file, int line, const char* call, VkResult result);
}

#define OBLO_VK_CHECK(Call) if (const VkResult result = Call; result != VK_SUCCESS) { ::oblo::vulkan_fatal_error(__FILE__, __LINE__, #Call, result); }