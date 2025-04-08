#pragma once

#include <oblo/core/string/debug_label.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/draw/types.hpp>

#include <span>

namespace oblo::vk
{
    struct texture_resource_initializer
    {
        u32 width;
        u32 height;
        texture_format format;
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