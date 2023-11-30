#pragma once

#include <oblo/math/vec2.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/draw/buffer_binding_table.hpp>
#include <oblo/vulkan/graph/pins.hpp>

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
    struct texture;
    struct render_pass;

    struct picking_configuration
    {
        bool enabled;
        vec2 coordinates;
        buffer downloadBuffer;
    };

    struct forward_pass
    {
        data<picking_configuration> inPickingConfiguration;

        data<vec2u> inResolution;
        data<buffer_binding_table> inPerViewBindingTable;
        resource<texture> outRenderTarget;
        resource<texture> outPickingIdBuffer;
        resource<texture> outDepthBuffer;

        h32<render_pass> renderPass;

        h32<string> pickingEnabledDefine;

        // Cached in build to avoid re-accessing all the time
        bool isPickingEnabled;

        void build(const runtime_builder& builder);

        void init(const init_context& context);

        void execute(const runtime_context& context);
    };
}