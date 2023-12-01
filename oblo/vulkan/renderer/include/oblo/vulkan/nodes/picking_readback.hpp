#pragma once

#include <oblo/math/vec2.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo::vk
{
    class init_context;
    class runtime_builder;
    class runtime_context;

    struct buffer;
    struct texture;

    struct picking_configuration
    {
        bool enabled;
        vec2 coordinates;
        buffer downloadBuffer;
    };

    struct picking_readback
    {
        data<picking_configuration> inPickingConfiguration;

        resource<texture> inPickingIdBuffer;

        // Cached in build to avoid re-accessing all the time
        bool isPickingEnabled;

        void init(const init_context& context);

        void build(const runtime_builder& builder);

        void execute(const runtime_context& context);
    };
}