#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/types.hpp>

namespace oblo::vk
{
    struct gaussian_blur_config
    {
        u32 kernelSize{5};
        f32 sigma{1.f};

        static constexpr const char* get_shader_name()
        {
            return "gaussian_blur";
        }
    };

    void make_separable_blur_kernel(const gaussian_blur_config& cfg, dynamic_array<f32>& kernel);
}