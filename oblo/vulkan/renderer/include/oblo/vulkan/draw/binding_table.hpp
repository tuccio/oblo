#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/texture.hpp>

namespace oblo
{
    class string;
}

namespace oblo::vk
{
    enum class bindable_object_kind : u8
    {
        buffer,
        texture,
        acceleration_structure,
    };

    struct bindable_texture
    {
        VkImageView view;
    };

    struct bindable_acceleration_structure
    {
        VkAccelerationStructureKHR handle;
    };

    struct bindable_object
    {
        bindable_object_kind kind;

        union {
            buffer buffer;
            bindable_texture texture;
            bindable_acceleration_structure accelerationStructure;
        };
    };

    constexpr bindable_object make_bindable_object(const vk::buffer& b)
    {
        return {.kind = bindable_object_kind::buffer, .buffer = b};
    }

    constexpr bindable_object make_bindable_object(VkImageView view)
    {
        return {.kind = bindable_object_kind::texture, .texture = {view}};
    }

    constexpr bindable_object make_bindable_object(VkAccelerationStructureKHR accelerationStructure)
    {
        return {.kind = bindable_object_kind::acceleration_structure, .accelerationStructure = {accelerationStructure}};
    }

    using binding_table = flat_dense_map<h32<string>, bindable_object>;
}