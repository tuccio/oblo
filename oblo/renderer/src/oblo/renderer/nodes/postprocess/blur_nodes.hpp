#pragma once

#include <oblo/renderer/data/blur_configs.hpp>
#include <oblo/renderer/nodes/postprocess/separable_blur.hpp>

namespace oblo
{
    using gaussian_blur = separable_blur<gaussian_blur_config>;

    using box_blur = separable_blur<box_blur_config>;
}