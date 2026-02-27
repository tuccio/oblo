#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    struct gaussian_blur_config
    {
        u32 kernelSize;
        f32 sigma;

        static constexpr const char* get_shader_name()
        {
            return "gaussian_blur";
        }
    };

    struct box_blur_config
    {
        u32 kernelSize;

        static constexpr const char* get_shader_name()
        {
            return "box_blur";
        }
    };

    void make_separable_blur_kernel(const gaussian_blur_config& cfg, dynamic_array<f32>& kernel);
    void make_separable_blur_kernel(const box_blur_config& cfg, dynamic_array<f32>& kernel);
}