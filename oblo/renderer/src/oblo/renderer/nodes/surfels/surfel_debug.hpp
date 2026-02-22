#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>

#include <span>

namespace oblo
{
    struct surfel_debug
    {
        enum class mode : u8
        {
            surfel_grid_id,
            surfel_lighting,
            surfel_raycount,
            surfel_inconsistency,
            surfel_lifetime,
            enum_max,
        };

        pin::data<mode> inMode;

        pin::buffer inCameraBuffer;

        pin::texture inImage;
        pin::texture outDebugImage;

        pin::buffer inSurfelsData;
        pin::buffer inSurfelsSpawnData;
        pin::buffer inSurfelsGrid;
        pin::buffer inSurfelsGridData;
        pin::buffer inSurfelsLightingData;
        pin::buffer inSurfelsLightEstimatorData;

        pin::texture inVisibilityBuffer;
        pin::buffer inMeshDatabase;
        pin::buffer inInstanceTables;
        pin::data<instance_data_table_buffers_span> inInstanceBuffers;

        h32<compute_pass> debugPass;
        h32<compute_pass_instance> debugPassInstance;

        dynamic_array<f32> sphereGeometryData;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}