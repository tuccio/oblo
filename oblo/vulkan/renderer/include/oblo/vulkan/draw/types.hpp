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

    enum class attachment_load_op : u8
    {
        none,
        load,
        clear,
        dont_care,
    };

    enum class attachment_store_op : u8
    {
        none,
        store,
        dont_care,
    };
}