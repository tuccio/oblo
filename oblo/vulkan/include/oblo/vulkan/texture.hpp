#pragma once

#include <oblo/vulkan/allocator.hpp>

namespace oblo::vk
{
    struct texture : allocated_image, image_initializer
    {
    };
}