#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/iterator/flags_range.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/gpu/enums.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu::vk
{
    inline VkFormat convert_enum(gpu::data_format format)
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

        case buffer_usage::acceleration_structure_build_input:
            return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

        case buffer_usage::acceleration_structure_storage:
            return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

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

    inline VkFilter convert_enum(sampler_filter filter)
    {
        switch (filter)
        {
        case sampler_filter::nearest:
            return VK_FILTER_NEAREST;
        case sampler_filter::linear:
            return VK_FILTER_LINEAR;
        default:
            unreachable();
        }
    }

    inline VkSamplerAddressMode convert_enum(sampler_address_mode mode)
    {
        switch (mode)
        {
        case sampler_address_mode::repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case sampler_address_mode::mirrored_repeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case sampler_address_mode::clamp_to_edge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case sampler_address_mode::clamp_to_border:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case sampler_address_mode::mirror_clamp_to_edge:
            return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default:
            unreachable();
        }
    }

    inline VkMemoryPropertyFlagBits convert_enum(memory_requirement r)
    {
        switch (r)
        {
        case memory_requirement::host_visible:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        case memory_requirement::device_local:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        default:
            unreachable();
        }
    }

    inline VkSamplerMipmapMode convert_enum(sampler_mipmap_mode mode)
    {
        switch (mode)
        {
        case sampler_mipmap_mode::nearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case sampler_mipmap_mode::linear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default:
            unreachable();
        }
    }

    inline VkVertexInputRate convert_enum(vertex_input_rate rate)
    {
        switch (rate)
        {
        case vertex_input_rate::instance:
            return VK_VERTEX_INPUT_RATE_INSTANCE;

        case vertex_input_rate::vertex:
            return VK_VERTEX_INPUT_RATE_VERTEX;

        default:
            unreachable();
        }
    }

    inline VkShaderStageFlagBits convert_enum(shader_stage stage)
    {
        switch (stage)
        {
        case shader_stage::vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;

        case shader_stage::geometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;

        case shader_stage::fragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;

        case shader_stage::compute:
            return VK_SHADER_STAGE_COMPUTE_BIT;

        case shader_stage::raygen:
            return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case shader_stage::miss:
            return VK_SHADER_STAGE_MISS_BIT_KHR;

        case shader_stage::closest_hit:
            return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

        case shader_stage::intersection:
            return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

        case shader_stage::callable:
            return VK_SHADER_STAGE_CALLABLE_BIT_KHR;

        case shader_stage::mesh:
            return VK_SHADER_STAGE_MESH_BIT_EXT;

        case shader_stage::task:
            return VK_SHADER_STAGE_TASK_BIT_EXT;

        case shader_stage::tessellation_control:
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

        case shader_stage::tessellation_evaluation:
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

        case shader_stage::any_hit:
            return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

        default:
            unreachable();
        }
    }

    inline VkDescriptorType convert_enum(resource_binding_kind kind)
    {
        switch (kind)
        {
        case resource_binding_kind::uniform:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case resource_binding_kind::storage_buffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case resource_binding_kind::storage_image:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case resource_binding_kind::sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case resource_binding_kind::sampler_image:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case resource_binding_kind::acceleration_structure:
            return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
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

    inline VkMemoryPropertyFlags convert_enum_flags(flags<memory_requirement> r)
    {
        return detail::convert_enum_flags_impl<VkMemoryPropertyFlags>(r);
    }

    inline VkShaderStageFlags convert_enum_flags(flags<shader_stage> r)
    {
        return detail::convert_enum_flags_impl<VkShaderStageFlags>(r);
    }
}