#pragma once

#include <oblo/core/handle.hpp>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    struct texture;
}

namespace oblo::graphics
{
    struct viewport_component
    {
        u32 width;
        u32 height;
        h32<vk::texture> texture;
    };
}