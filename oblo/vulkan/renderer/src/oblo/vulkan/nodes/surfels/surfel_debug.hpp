#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

#include <span>

namespace oblo::vk
{
    struct surfel_debug
    {
        enum class mode : u8
        {
            surfel_grid_id,
            surfel_lighting,
            surfel_raycount,
            surfel_inconsistency,
            enum_max,
        };

        data<mode> inMode;

        resource<buffer> inCameraBuffer;

        resource<texture> inImage;
        resource<texture> outDebugImage;

        resource<buffer> inSurfelsData;
        resource<buffer> inSurfelsGrid;
        resource<buffer> inSurfelsGridData;
        resource<buffer> inSurfelsLightingData;
        resource<buffer> inSurfelsLightEstimatorData;

        resource<texture> inVisibilityBuffer;
        resource<buffer> inMeshDatabase;
        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        h32<compute_pass> debugPass;
        h32<compute_pass_instance> debugPassInstance;

        dynamic_array<f32> sphereGeometryData;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}