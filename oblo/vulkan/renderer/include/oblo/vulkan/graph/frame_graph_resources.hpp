#pragma once

#include <oblo/core/string/debug_label.hpp>
#include <oblo/core/types.hpp>

#include <span>

#include <vulkan/vulkan_core.h>

namespace oblo::vk
{
    struct texture_resource_initializer
    {
        u32 width;
        u32 height;
        VkFormat format;
        VkImageUsageFlags usage;
        bool isStable;
        debug_label debugLabel{std::source_location::current()};
    };

    struct buffer_resource_initializer
    {
        u32 size{};
        std::span<const byte> data;
        bool isStable{};
    };
}