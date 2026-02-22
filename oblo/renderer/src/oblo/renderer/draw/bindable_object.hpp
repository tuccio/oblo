#pragma once

#include <oblo/renderer/draw/binding_table.hpp>
#include <oblo/renderer/platform/renderer_platform.hpp>

namespace oblo
{
    struct bindable_buffer
    {
        VkBuffer buffer;
        VkDeviceSize offset;
        VkDeviceSize size;
    };

    struct bindable_texture
    {
        VkImageView view;
        VkImageLayout layout;
    };

    struct bindable_acceleration_structure
    {
        VkAccelerationStructureKHR handle;
    };

    struct bindable_object
    {
        bindable_resource_kind kind;

        union {
            bindable_buffer buffer;
            bindable_texture texture;
            bindable_acceleration_structure accelerationStructure;
        };
    };

    constexpr bindable_object make_bindable_object(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size)
    {
        return {.kind = bindable_resource_kind::buffer, .buffer = {buffer, offset, size}};
    }

    constexpr bindable_object make_bindable_object(VkImageView view, VkImageLayout layout)
    {
        return {.kind = bindable_resource_kind::texture, .texture = {view, layout}};
    }

    constexpr bindable_object make_bindable_object(VkAccelerationStructureKHR accelerationStructure)
    {
        return {.kind = bindable_resource_kind::acceleration_structure,
            .accelerationStructure = {accelerationStructure}};
    }
}