#include <oblo/core/expected.hpp>

namespace oblo::vk
{
    template <typename T>
    using vk_result = expected<T, VkResult>;
}