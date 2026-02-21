#pragma once

#include <oblo/vulkan/data/blur_configs.hpp>
#include <oblo/vulkan/nodes/postprocess/separable_blur.hpp>

namespace oblo
{
    using gaussian_blur = separable_blur<gaussian_blur_config>;

    using box_blur = separable_blur<box_blur_config>;
}