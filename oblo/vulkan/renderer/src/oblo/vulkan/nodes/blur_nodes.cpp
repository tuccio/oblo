#include <oblo/vulkan/nodes/blur_nodes.hpp>

#include <oblo/vulkan/nodes/separable_blur_impl.hpp>

namespace oblo::vk
{
    template struct separable_blur<gaussian_blur_config, separable_blur_pass::horizontal>;
    template struct separable_blur<gaussian_blur_config, separable_blur_pass::vertical>;
}