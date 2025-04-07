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

    enum class texture_usage : u8
    {
        render_target_write,
        depth_stencil_read,
        depth_stencil_write,
        shader_read,
        storage_read,
        storage_write,
        transfer_source,
        transfer_destination,
        present,
    };

    enum class buffer_usage : u8
    {
        storage_read,
        storage_write,
        /// @brief This means the buffer is not actually used on GPU in this node, just uploaded on.
        storage_upload,
        uniform,
        indirect,
        download,
        index,
        enum_max,
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