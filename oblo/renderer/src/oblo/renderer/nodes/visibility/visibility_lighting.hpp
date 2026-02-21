#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/data/light_data.hpp>
#include <oblo/renderer/data/light_visibility_event.hpp>
#include <oblo/renderer/data/skybox_settings.hpp>
#include <oblo/renderer/data/visibility_debug_mode.hpp>
#include <oblo/renderer/draw/binding_table.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>

#include <span>

namespace oblo
{
    struct visibility_lighting
    {
        data<vec2u> inResolution;

        data<std::span<const light_data>> inLights;

        data_sink<light_visibility_event> inShadowSink;

        resource<buffer> inCameraBuffer;
        resource<buffer> inSkyboxSettingsBuffer;

        resource<buffer> inLightBuffer;
        resource<buffer> inLightConfig;

        resource<buffer> inSurfelsGrid;
        resource<buffer> inSurfelsGridData;
        resource<buffer> inSurfelsData;
        resource<buffer> inSurfelsLightingData;
        resource<buffer> inOutSurfelsLastUsage;

        // We might not use it for now, but we still forward it to debug views
        resource<buffer> inSurfelsLightEstimatorData;

        resource<buffer> inMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<texture> inVisibilityBuffer;
        resource<texture> inAmbientOcclusion;
        resource<texture> outShadedImage;

        resource<buffer> outShadowMaps;

        h32<compute_pass> lightingPass;
        h32<compute_pass_instance> lightingPassInstance;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };

    // TODO: (#56) The standard layout requirement for nodes forces some copy paste.

    struct visibility_debug
    {
        data<vec2u> inResolution;
        data<visibility_debug_mode> inDebugMode;

        resource<buffer> inCameraBuffer;

        resource<buffer> inMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<texture> inVisibilityBuffer;
        resource<texture> outShadedImage;

        h32<compute_pass> debugPass;
        h32<compute_pass_instance> debugPassInstance;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}