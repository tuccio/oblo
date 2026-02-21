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

        pin::buffer inCameraBuffer;
        pin::buffer inSkyboxSettingsBuffer;

        pin::buffer inLightBuffer;
        pin::buffer inLightConfig;

        pin::buffer inSurfelsGrid;
        pin::buffer inSurfelsGridData;
        pin::buffer inSurfelsData;
        pin::buffer inSurfelsLightingData;
        pin::buffer inOutSurfelsLastUsage;

        // We might not use it for now, but we still forward it to debug views
        pin::buffer inSurfelsLightEstimatorData;

        pin::buffer inMeshDatabase;

        pin::buffer inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        pin::texture inVisibilityBuffer;
        pin::texture inAmbientOcclusion;
        pin::texture outShadedImage;

        pin::buffer outShadowMaps;

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

        pin::buffer inCameraBuffer;

        pin::buffer inMeshDatabase;

        pin::buffer inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        pin::texture inVisibilityBuffer;
        pin::texture outShadedImage;

        h32<compute_pass> debugPass;
        h32<compute_pass_instance> debugPassInstance;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}