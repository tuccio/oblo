#include <oblo/vulkan/nodes/postprocess/blur_nodes.hpp>

#include <oblo/vulkan/nodes/postprocess/separable_blur_impl.hpp>

namespace oblo::vk
{
    template struct separable_blur<gaussian_blur_config, separable_blur_pass::horizontal>;
    template struct separable_blur<gaussian_blur_config, separable_blur_pass::vertical>;

    template struct separable_blur<box_blur_config, separable_blur_pass::horizontal>;
    template struct separable_blur<box_blur_config, separable_blur_pass::vertical>;
}