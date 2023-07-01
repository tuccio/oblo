#pragma once

#include <oblo/vulkan/resource_manager.hpp>

namespace oblo::vk
{
    struct stateful_command_buffer
    {
        VkCommandBuffer commandBuffer;
        command_buffer_state state;
    };
}