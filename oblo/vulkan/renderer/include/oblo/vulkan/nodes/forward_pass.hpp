#pragma once

#include <oblo/vulkan/draw/buffer_binding_table.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <span>

namespace oblo
{
    struct string;
    struct vec2u;
}

namespace oblo::vk
{
    class init_context;
    class runtime_builder;
    class runtime_context;

    struct buffer;
    struct draw_buffer_data;
    struct texture;
    struct picking_configuration;
    struct render_pass;

    struct forward_pass
    {
        data<picking_configuration> inPickingConfiguration;

        data<vec2u> inResolution;
        data<buffer_binding_table> inPerViewBindingTable;
        data<std::span<draw_buffer_data>> inDrawData;

        resource<texture> outRenderTarget;
        resource<texture> outPickingIdBuffer;
        resource<texture> outDepthBuffer;

        h32<render_pass> renderPass;

        h32<string> pickingEnabledDefine;

        bool isPickingEnabled;

        void init(const init_context& context);

        void build(const runtime_builder& builder);

        void execute(const runtime_context& context);

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}