#pragma once

#include <oblo/vulkan/data/blur_configs.hpp>
#include <oblo/vulkan/nodes/postprocess/separable_blur.hpp>

namespace oblo::vk
{
    using gaussian_blur_h = separable_blur<gaussian_blur_config, separable_blur_pass::horizontal>;
    using gaussian_blur_v = separable_blur<gaussian_blur_config, separable_blur_pass::vertical>;

    using box_blur_h = separable_blur<box_blur_config, separable_blur_pass::horizontal>;
    using box_blur_v = separable_blur<box_blur_config, separable_blur_pass::vertical>;
}