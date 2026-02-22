#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/iterator/flags_range.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/gpu/types.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu::vk
{
    inline VkFormat convert_enum(gpu::image_format format)
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

    inline VkImageUsageFlagBits convert_enum(image_usage usage)
    {
        switch (usage)
        {
        case image_usage::depth_stencil:
            return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        case image_usage::color_attachment:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        case image_usage::shader_sample:
            return VK_IMAGE_USAGE_SAMPLED_BIT;

        case image_usage::transfer_destination:
            return VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        case image_usage::transfer_source:
            return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        case image_usage::storage:
            return VK_IMAGE_USAGE_STORAGE_BIT;

        default:
            unreachable();
        };
    }

    inline VkBufferUsageFlagBits convert_enum(buffer_usage usage)
    {
        OBLO_ASSERT(usage < buffer_usage::enum_max);

        switch (usage)
        {
        case buffer_usage::storage:
            return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        case buffer_usage::uniform:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        case buffer_usage::vertex:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        case buffer_usage::index:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        case buffer_usage::indirect:
            return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

        case buffer_usage::transfer_source:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        case buffer_usage::transfer_destination:
            return VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        case buffer_usage::device_address:
            return VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        default:
            unreachable();
        }
    }

    inline VkImageAspectFlagBits convert_enum(image_aspect usage)
    {
        OBLO_ASSERT(usage < image_aspect::enum_max);

        switch (usage)
        {
        case image_aspect::color:
            return VK_IMAGE_ASPECT_COLOR_BIT;

        case image_aspect::depth:
            return VK_IMAGE_ASPECT_DEPTH_BIT;

        default:
            unreachable();
        }
    }

    namespace detail
    {
        template <typename R, typename T>
        OBLO_FORCEINLINE R convert_enum_flags_impl(flags<T> usages)
        {
            R r{};

            for (const T usage : flags_range{usages})
            {
                r |= convert_enum(usage);
            }

            return r;
        }
    }

    inline VkImageUsageFlags convert_enum_flags(flags<image_usage> usages)
    {
        return detail::convert_enum_flags_impl<VkImageUsageFlags>(usages);
    }

    inline VkBufferUsageFlags convert_enum_flags(flags<buffer_usage> usages)
    {
        return detail::convert_enum_flags_impl<VkBufferUsageFlags>(usages);
    }

    inline VkImageAspectFlags convert_enum_flags(flags<image_aspect> aspectMask)
    {
        return detail::convert_enum_flags_impl<VkImageAspectFlags>(aspectMask);
    }
}