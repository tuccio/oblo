#pragma once

#include <oblo/core/handle.hpp>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    class render_graph;
    struct texture;
}

namespace oblo::graphics
{
    struct viewport_component
    {
        u32 width;
        u32 height;
        h32<vk::texture> texture;
        h32<vk::render_graph> renderGraph;
    };
}