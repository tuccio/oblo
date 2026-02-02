#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/gpu/types.hpp>
#include <oblo/vulkan/gpu_temporary_aliases.hpp>

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

    inline VkAttachmentLoadOp convert_to_vk(attachment_load_op op)
    {
        switch (op)
        {
        case attachment_load_op::none:
            return VK_ATTACHMENT_LOAD_OP_NONE_KHR;
        case attachment_load_op::load:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case attachment_load_op::clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case attachment_load_op::dont_care:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default:
            unreachable();
        }
    }

    inline VkAttachmentStoreOp convert_to_vk(attachment_store_op op)
    {
        switch (op)
        {
        case attachment_store_op::none:
            return VK_ATTACHMENT_STORE_OP_NONE;
        case attachment_store_op::store:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case attachment_store_op::dont_care:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default:
            unreachable();
        }
    }

    inline VkCompareOp convert_to_vk(compare_op op)
    {
        return VkCompareOp(op);
    }

    inline VkStencilOp convert_to_vk(stencil_op op)
    {
        return VkStencilOp(op);
    }

    inline VkPrimitiveTopology convert_to_vk(primitive_topology topology)
    {
        return VkPrimitiveTopology(topology);
    }

    inline VkPolygonMode convert_to_vk(polygon_mode mode)
    {
        return VkPolygonMode(mode);
    }

    inline VkCullModeFlags convert_to_vk(flags<cull_mode> f)
    {
        return VkCullModeFlags(f.storage);
    }

    inline VkFrontFace convert_to_vk(front_face face)
    {
        return VkFrontFace(face);
    }

    inline VkPipelineDepthStencilStateCreateFlags convert_to_vk(flags<pipeline_depth_stencil_state_create> f)
    {
        return VkPipelineDepthStencilStateCreateFlags(f.storage);
    }

    inline VkFormat convert_to_vk(texture_format format)
    {
        return VkFormat(format);
    }

    inline texture_format convert_to_oblo(VkFormat format)
    {
        return texture_format(format);
    }

    inline VkBlendFactor convert_to_vk(blend_factor factor)
    {
        return VkBlendFactor(factor);
    }

    inline VkBlendOp convert_to_vk(blend_op op)
    {
        return VkBlendOp(op);
    }

    inline VkColorComponentFlags convert_to_vk(flags<color_component> f)
    {
        return VkColorComponentFlags(f.storage);
    }
}