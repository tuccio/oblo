#pragma once

#include <oblo/core/types.hpp>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    void setup_viewport_scissor(VkCommandBuffer commandBuffer, u32 width, u32 height);
}