#pragma once

#include <oblo/gpu/types.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu::vk
{
    inline VkFormat convert_enum(texture_format format)
    {
        return VkFormat(format);
    }
}