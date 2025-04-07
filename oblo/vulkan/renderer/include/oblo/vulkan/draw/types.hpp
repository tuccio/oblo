#pragma once

#include <oblo/core/types.hpp>

namespace oblo::vk
{
    enum class mesh_index_type : u8
    {
        none,
        u8,
        u16,
        u32,
    };
}