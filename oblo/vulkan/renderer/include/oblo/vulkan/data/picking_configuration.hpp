#pragma once

#include <oblo/math/vec2.hpp>
#include <oblo/vulkan/buffer.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::vk
{
    struct picking_configuration
    {
        vec2 coordinates;
    };
}