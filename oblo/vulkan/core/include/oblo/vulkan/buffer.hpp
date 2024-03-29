#pragma once

#include <oblo/vulkan/gpu_allocator.hpp>

namespace oblo::vk
{
    struct buffer
    {
        VkBuffer buffer;
        u32 offset;
        u32 size;
        VmaAllocation allocation;
    };
}