#pragma once

#include <oblo/core/unreachable.hpp>
#include <oblo/vulkan/draw/types.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::vk
{
    inline VkIndexType convert_to_vk(mesh_index_type indexType)
    {
        switch (indexType)
        {
        case mesh_index_type::none:
            return VK_INDEX_TYPE_NONE_KHR;

        case mesh_index_type::u8:
            return VK_INDEX_TYPE_UINT8_EXT;

        case mesh_index_type::u16:
            return VK_INDEX_TYPE_UINT16;

        case mesh_index_type::u32:
            return VK_INDEX_TYPE_UINT32;

        default:
            unreachable();
        }
    }

    inline mesh_index_type convert_to_oblo(VkIndexType indexType)
    {
        switch (indexType)
        {
        case VK_INDEX_TYPE_NONE_KHR:
            return mesh_index_type::none;

        case VK_INDEX_TYPE_UINT8_EXT:
            return mesh_index_type::u8;

        case VK_INDEX_TYPE_UINT16:
            return mesh_index_type::u16;

        case VK_INDEX_TYPE_UINT32:
            return mesh_index_type::u32;

        default:
            unreachable();
        }
    }
}