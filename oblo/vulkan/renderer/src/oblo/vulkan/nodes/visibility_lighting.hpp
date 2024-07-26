#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/light_data.hpp>
#include <oblo/vulkan/data/light_visibility_event.hpp>
#include <oblo/vulkan/data/visibiility_debug_mode.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/instance_table_node.hpp>

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

        resource<buffer> inLightBuffer;
        resource<buffer> inLightConfig;

        resource<buffer> inMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<texture> inVisibilityBuffer;
        resource<texture> outShadedImage;

        resource<buffer> outShadowMaps;

        h32<compute_pass> lightingPass;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
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

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}