#pragma once

#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <span>

namespace oblo::vk
{
    struct draw_buffer_data;

    struct bypass_culling
    {
        data<std::span<draw_buffer_data>> outDrawBufferData;

        void build(const frame_graph_build_context& builder);
    };
}