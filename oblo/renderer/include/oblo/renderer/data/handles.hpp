#pragma once

#include <oblo/core/handle.hpp>

namespace oblo
{
    struct gpu_mesh;

    class mesh_table;

    // We use 24 bits for the mesh index, 8 bits for mesh tables
    using mesh_handle = h32<gpu_mesh>;

    using mesh_table_handle = h8<mesh_table>;
}