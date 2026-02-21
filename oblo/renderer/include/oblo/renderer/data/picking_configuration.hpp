#pragma once

#include <oblo/math/vec2.hpp>
#include <oblo/renderer/buffer.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo
{
    struct picking_configuration
    {
        vec2 coordinates;
    };
}