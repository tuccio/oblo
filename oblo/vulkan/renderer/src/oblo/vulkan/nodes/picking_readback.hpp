#pragma once

#include <oblo/math/vec2.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo::vk
{
    struct picking_readback
    {
        data<picking_configuration> inPickingConfiguration;

        resource<texture> inPickingIdBuffer;

        // Cached in build to avoid re-accessing all the time
        bool isPickingEnabled;

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}