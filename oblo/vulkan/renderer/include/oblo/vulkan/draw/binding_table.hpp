#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/texture.hpp>

namespace oblo
{
    struct string;
}

namespace oblo::vk
{
    enum class bindable_object_kind : u8
    {
        buffer,
        texture,
    };

    struct bindable_object
    {
        bindable_object_kind kind;

        union {
            buffer buffer;
            texture texture;
        };
    };

    constexpr bindable_object make_bindable_object(const vk::buffer& b)
    {
        return {.kind = bindable_object_kind::buffer, .buffer = b};
    }

    constexpr bindable_object make_bindable_object(const vk::texture& t)
    {
        return {.kind = bindable_object_kind::texture, .texture = t};
    }

    using binding_table = flat_dense_map<h32<string>, bindable_object>;
}