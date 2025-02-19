#pragma once

#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

#include <span>

namespace oblo::vk
{
    struct surfel_lighting
    {
        resource<buffer> inCameraBuffer;

        resource<buffer> inSurfelsGrid;
        resource<buffer> inSurfelsGridData;
        resource<buffer> inSurfelsData;
        resource<buffer> inSurfelsLightingData;
        resource<buffer> inOutSurfelsLastUsage;

        resource<buffer> inMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<texture> inVisibilityBuffer;
        resource<texture> outIndirectLighting;
        resource<texture> filterAux;

        h32<compute_pass> lightingPass;
        h32<compute_pass_instance> lightingPassInstance;

        h32<compute_pass> filterPass;
        std::span<h32<compute_pass_instance>> filterPassInstances;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);

        void acquire_view_buffers(const frame_graph_build_context& ctx) const;
    };
}