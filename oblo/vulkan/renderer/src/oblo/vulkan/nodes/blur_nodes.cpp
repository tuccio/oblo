#include <oblo/vulkan/nodes/blur_nodes.hpp>

#include <oblo/vulkan/nodes/separable_blur_impl.hpp>

namespace oblo::vk
{
    template struct separable_blur<gaussian_blur_config, 0>;
    template struct separable_blur<gaussian_blur_config, 1>;
}