#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/gpu/error.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu
{
    inline error translate_reportable_error(VkResult result)
    {
        switch (result)
        {
        case VK_SUCCESS:
            OBLO_ASSERT(false);
            return error::undefined;

        case VK_ERROR_DEVICE_LOST:
            return error::device_lost;

        default:
            return error::undefined;
        }
    }

    inline result<> translate_result(VkResult result)
    {
        if (result == VK_SUCCESS)
        {
            return no_error;
        }

        return translate_reportable_error(result);
    }

    template <typename V>
    result<V> translate_error_or_value(VkResult result, const V& value)
    {
        if (result == VK_SUCCESS)
        {
            return value;
        }

        return translate_reportable_error(result);
    }
}