#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/vulkan/buffer.hpp>

namespace oblo
{
    struct string;
}

namespace oblo::vk
{
    struct buffer;

    using buffer_binding_table = flat_dense_map<h32<string>, buffer>;
}