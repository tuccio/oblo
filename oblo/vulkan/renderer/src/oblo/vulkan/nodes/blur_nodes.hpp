#pragma once

#include <oblo/vulkan/data/blur_configs.hpp>
#include <oblo/vulkan/nodes/separable_blur.hpp>

namespace oblo::vk
{
    using gaussian_blur_h = separable_blur<gaussian_blur_config, 0>;
    using gaussian_blur_v = separable_blur<gaussian_blur_config, 1>;
}