#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/light_data.hpp>
#include <oblo/vulkan/data/light_visibility_event.hpp>
#include <oblo/vulkan/data/skybox_settings.hpp>
#include <oblo/vulkan/data/visibility_debug_mode.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

#include <span>

namespace oblo
{
    class string;
}

namespace oblo::vk
{
    struct draw_buffer_data;
    struct picking_configuration;

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
        resource<buffer> inSurfelsData;

        resource<buffer> inMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<texture> inVisibilityBuffer;
        resource<texture> outShadedImage;

        resource<buffer> outShadowMaps;

        h32<compute_pass> lightingPass;

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

        h32<compute_pass> albedoPass;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}