#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/iterator/flags_range.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/gpu/types.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu::vk
{
    inline VkFormat convert_enum(texture_format format)
    {
        return VkFormat(format);
    }

    inline VkSampleCountFlagBits convert_enum(samples_count samples)
    {
        switch (samples)
        {
        case samples_count::one:
            return VK_SAMPLE_COUNT_1_BIT;

        default:
            return VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
        }
    }

    inline VkImageType convert_image_type(image_type type)
    {
        switch (type)
        {
        case image_type::plain_2d:
            return VK_IMAGE_TYPE_2D;

        case image_type::plain_3d:
            return VK_IMAGE_TYPE_3D;

        case image_type::cubemap:
            return VK_IMAGE_TYPE_2D;

        default:
            unreachable();
        }
    }

    inline VkImageViewType convert_image_view_type(image_type type)
    {
        switch (type)
        {
        case image_type::plain_2d:
            return VK_IMAGE_VIEW_TYPE_2D;

        case image_type::plain_3d:
            return VK_IMAGE_VIEW_TYPE_3D;

        case image_type::cubemap:
            return VK_IMAGE_VIEW_TYPE_CUBE;

        default:
            unreachable();
        }
    }

    inline VkImageUsageFlagBits convert_enum(texture_usage usage)
    {
        switch (usage)
        {
        case texture_usage::depth_stencil_read:
        case texture_usage::depth_stencil_write:
            return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        case texture_usage::render_target_write:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        case texture_usage::shader_read:
            return VK_IMAGE_USAGE_SAMPLED_BIT;

        case texture_usage::transfer_destination:
            return VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        case texture_usage::transfer_source:
            return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        case texture_usage::storage_read:
        case texture_usage::storage_write:
            return {};

        default:
            unreachable();
        };
    }

    inline VkImageUsageFlags convert_enum_flags(flags<texture_usage> usages)
    {
        VkImageUsageFlags r{};

        for (const texture_usage usage : flags_range{usages})
        {
            r |= convert_enum(usage);
        }

        return r;
    }

    inline VkBufferUsageFlagBits convert_enum(buffer_usage usage)
    {
        OBLO_ASSERT(usage != buffer_usage::enum_max);

        switch (usage)
        {
        case buffer_usage::storage_read:
        case buffer_usage::storage_write:
        case buffer_usage::storage_upload:
            return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        case buffer_usage::indirect:
            return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

        case buffer_usage::uniform:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        case buffer_usage::download:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        case buffer_usage::index:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        default:
            unreachable();
        }
    }

    inline VkBufferUsageFlags convert_enum_flags(flags<buffer_usage> usages)
    {
        VkBufferUsageFlags r{};

        for (const buffer_usage usage : flags_range{usages})
        {
            r |= convert_enum(usage);
        }

        return r;
    }
}